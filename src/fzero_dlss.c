#include "fzero_dlss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>

/* One in-flight frame. Events transfer ownership of the shared buffers;
 * simulation never waits on the neural worker. Keep this ABI in worker.py. */
enum { MAX_PIXELS = 1280 * 960, HEADER_BYTES = 1024,
       MAPPING_BYTES = HEADER_BYTES + MAX_PIXELS * 8 };
static HANDLE mapping, request, done, stop, process, job;
static unsigned char *shared;
static uint32_t *cached;
static uint32_t *original;
static const char *status = "Off";
static bool busy, valid, reset = true;
static unsigned generation, submitted_generation;
static int cached_width, cached_height;
static ULONGLONG submitted_at;

void FzeroDlssStop(void) {
  if (stop) SetEvent(stop);
  if (process) {
    if (WaitForSingleObject(process, 2000) == WAIT_TIMEOUT) {
      /* Only our disposable worker is terminated, never the game or driver. */
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 2000);
    }
    CloseHandle(process);
  }
  if (shared) UnmapViewOfFile(shared);
  if (mapping) CloseHandle(mapping);
  if (request) CloseHandle(request);
  if (done) CloseHandle(done);
  if (stop) CloseHandle(stop);
  if (job) CloseHandle(job);
  free(cached); free(original);
  mapping = request = done = stop = process = job = NULL;
  shared = NULL; cached = original = NULL; busy = valid = false; reset = true;
  status = "Off";
}

const char *FzeroDlssStatus(void) { return status; }
const uint32_t *FzeroDlssOriginal(void) { return valid ? original : NULL; }

bool FzeroDlssStart(void) {
  if (process) return true;
  char directory[MAX_PATH], default_python[MAX_PATH * 2], default_root[MAX_PATH * 2];
  DWORD path_length = GetModuleFileNameA(NULL, directory, sizeof(directory));
  if (!path_length || path_length >= sizeof(directory)) return false;
  char *directory_slash = strrchr(directory, '\\');
  if (!directory_slash) return false;
  *directory_slash = 0;
  snprintf(default_python, sizeof(default_python), "%s\\dlss-deps\\venv\\Scripts\\python.exe", directory);
  snprintf(default_root, sizeof(default_root), "%s\\dlss-deps\\bridge\\ComfyUI-DLSS5-NR", directory);
  const char *python = getenv("FZERO_DLSS_PYTHON");
  const char *root = getenv("FZERO_DLSS_ROOT");
  if (!python) python = default_python;
  if (!root) root = default_root;
  if (strchr(python, '"') || strchr(root, '"')) {
    fprintf(stderr, "[dlss] Set FZERO_DLSS_PYTHON and FZERO_DLSS_ROOT using the experiment launcher\n");
    return false;
  }
  char base[128], name[160], exe[MAX_PATH], command[4096];
  snprintf(base, sizeof(base), "Local\\FZeroNR-%lu-%llu", GetCurrentProcessId(), GetTickCount64());
  mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAPPING_BYTES, base);
  if (!mapping) goto fail;
  shared = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, MAPPING_BYTES);
  snprintf(name, sizeof(name), "%s-request", base); request = CreateEventA(NULL, FALSE, FALSE, name);
  snprintf(name, sizeof(name), "%s-done", base); done = CreateEventA(NULL, FALSE, FALSE, name);
  snprintf(name, sizeof(name), "%s-stop", base); stop = CreateEventA(NULL, TRUE, FALSE, name);
  cached = malloc(MAX_PIXELS * sizeof(*cached));
  original = malloc(MAX_PIXELS * sizeof(*original));
  if (!shared || !request || !done || !stop || !cached || !original) goto fail;
  DWORD length = GetModuleFileNameA(NULL, exe, sizeof(exe));
  if (!length || length >= sizeof(exe)) goto fail;
  char *slash = strrchr(exe, '\\');
  if (!slash) goto fail;
  *slash = 0;
  if (snprintf(command, sizeof(command), "\"%s\" \"%s\\dlss_worker.py\" live --root \"%s\" --mapping \"%s\"",
               python, exe, root, base) >= (int)sizeof(command)) goto fail;
  STARTUPINFOA si = {0}; si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {0};
  job = CreateJobObjectA(NULL, NULL);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) goto fail;
  if (!CreateProcessA(python, command, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, exe, &si, &pi)) goto fail;
  process = pi.hProcess;
  if (!AssignProcessToJobObject(job, process)) {
    CloseHandle(pi.hThread); goto fail;
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  busy = true; submitted_at = GetTickCount64();
  reset = true; valid = false;
  status = "Starting";
  fprintf(stderr, "[dlss] Neural worker starting\n");
  return true;
fail:
  fprintf(stderr, "[dlss] Worker startup failed: Win32 %lu\n", GetLastError());
  FzeroDlssStop(); status = "Failed (original output)"; return false;
}

void FzeroDlssReset(void) { ++generation; valid = false; reset = true; }

const uint32_t *FzeroDlssFrame(const uint32_t *pixels, int width, int height,
                              double aspect, int *out_width, int *out_height) {
  if (!process) return NULL;
  uint32_t *header = (uint32_t *)shared;
  if (busy && WaitForSingleObject(done, 0) == WAIT_OBJECT_0) {
    busy = false;
    if ((int32_t)header[3] < 0) {
      fprintf(stderr, "[dlss] Worker error: %.900s\n", shared + 64);
      FzeroDlssStop(); status = "Failed (original output)"; return NULL;
    }
    if (header[3] == 2 && submitted_generation == generation) {
      cached_width = (int)header[0]; cached_height = (int)header[1];
      if (cached_width <= 0 || cached_height <= 0 || cached_width > 1280 || cached_height > 960) {
        FzeroDlssStop(); return NULL;
      }
      memcpy(cached, shared + HEADER_BYTES + MAX_PIXELS * 4,
             (size_t)cached_width * cached_height * 4);
      memcpy(original, shared + HEADER_BYTES, (size_t)cached_width * cached_height * 4);
      valid = true;
      status = header[7] ? "Temporal" : "Priming history";
      fprintf(stderr, "[dlss] Neural frame %u: %u ms, %dx%d, %s\n", header[6], header[4], cached_width, cached_height, status);
    }
  }
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0 ||
      (busy && GetTickCount64() - submitted_at > 60000)) {
    DWORD code = 0; GetExitCodeProcess(process, &code);
    fprintf(stderr, "[dlss] Worker exited or timed out (code %lu); restoring original output\n", code);
    FzeroDlssStop(); status = "Failed (original output)"; return NULL;
  }
  if (!busy) {
    int h = 480;
    const char *height_env = getenv("FZERO_DLSS_HEIGHT");
    if (height_env) {
      int requested = atoi(height_env);
      if (requested >= 240 && requested <= 960) h = requested;
    }
    int w = (int)(aspect * h + 0.5);
    if (w > 1280) { w = 1280; h = (int)(w / aspect + 0.5); }
    uint32_t *input = (uint32_t *)(shared + HEADER_BYTES);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
      input[y * w + x] = pixels[(y * height / h) * width + x * width / w];
    header[0] = w; header[1] = h; header[2] = reset; ++header[5];
    reset = false; busy = true; submitted_generation = generation;
    submitted_at = GetTickCount64(); SetEvent(request);
  }
  *out_width = cached_width; *out_height = cached_height;
  return valid ? cached : NULL;
}
#else
bool FzeroDlssStart(void) { fprintf(stderr, "[dlss] This experiment requires Windows\n"); return false; }
const char *FzeroDlssStatus(void) { return "Unavailable"; }
const uint32_t *FzeroDlssOriginal(void) { return NULL; }
void FzeroDlssStop(void) {}
void FzeroDlssReset(void) {}
const uint32_t *FzeroDlssFrame(const uint32_t *p, int w, int h, double a, int *ow, int *oh) {
  (void)p; (void)w; (void)h; (void)a; (void)ow; (void)oh; return NULL;
}
#endif
