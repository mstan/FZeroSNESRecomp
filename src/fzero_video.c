#include "fzero_video.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const char *const aspect_names[] = {"4:3", "16:9", "21:9", "32:9", "Fit"};

void FzeroVideoDefaults(FzeroVideoSettings *s) {
  *s = (FzeroVideoSettings){.aspect = FZERO_ASPECT_16_9};
}

const char *FzeroAspectName(FzeroAspect aspect) {
  return aspect >= 0 && aspect < FZERO_ASPECT_COUNT ? aspect_names[aspect] : "4:3";
}

bool FzeroParseAspect(const char *text, FzeroAspect *aspect) {
  for (int i = 0; i < FZERO_ASPECT_COUNT; ++i) {
    if (strcmp(text, aspect_names[i]) == 0) {
      *aspect = (FzeroAspect)i;
      return true;
    }
  }
  return false;
}

bool FzeroValidFps(unsigned fps) {
  return fps == 0 || fps == 60 || fps == 90 || fps == 120 || fps == 144 ||
         fps == 165 || fps == 240 || fps == 360;
}

double FzeroPresentationHz(unsigned fps, double refresh) {
  if (!FzeroValidFps(fps)) fps = 0;
  if (fps) return fps;
  if (!isfinite(refresh) || refresh < 1) return 60;
  return fmin(refresh, 360);
}

FzeroViewport FzeroCalculateViewport(const FzeroVideoSettings *s, int w, int h) {
  double aspect = 4.0 / 3.0;
  if (s->enhanced) {
    switch (s->aspect) {
    case FZERO_ASPECT_16_9: aspect = 16.0 / 9.0; break;
    case FZERO_ASPECT_21_9: aspect = 21.0 / 9.0; break;
    case FZERO_ASPECT_32_9: aspect = 32.0 / 9.0; break;
    case FZERO_ASPECT_FIT:
      if (w > 0 && h > 0) aspect = fmax(4.0 / 3.0, fmin(32.0 / 9.0, (double)w / h));
      break;
    default: break;
    }
  }
  /* Stock pixels have display aspect 7:6. Keep that scale at every width;
   * round to an even width so the stock center remains exactly centered. */
  int width = 2 * (int)floor((256.0 * aspect / (4.0 / 3.0)) / 2.0 + 0.5);
  if (width < 256) width = 256;
  if (width > FZERO_MAX_WIDTH) width = FZERO_MAX_WIDTH;
  return (FzeroViewport){width, (width - 256) / 2, aspect, width > 256};
}

FzeroRect FzeroDestination(FzeroViewport viewport, int width, int height) {
  if (width <= 0 || height <= 0) return (FzeroRect){0, 0, 0, 0};
  int w = width, h = (int)floor(width / viewport.aspect + 0.5);
  if (h > height) { h = height; w = (int)floor(height * viewport.aspect + 0.5); }
  return (FzeroRect){(width - w) / 2, (height - h) / 2, w, h};
}

int FzeroHudAnchorX(FzeroViewport viewport, int x, int anchor) {
  return x + (anchor < 0 ? 0 : anchor > 0 ? 2 * viewport.extra : viewport.extra);
}

bool FzeroVideoLoad(FzeroVideoSettings *s, const char *path) {
  FzeroVideoDefaults(s);
  FILE *f = fopen(path, "r");
  if (!f) return errno == ENOENT;
  char line[256], key[64], value[64], tail;
  bool valid = true;
  bool has_fps_toggle = false;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, " %63[^= \t] = %63s", key, value) != 2) continue;
    if (!strcmp(key, "Vulkan") || !strcmp(key, "DLSS5")) {
      if (!strcmp(value, "0") || !strcmp(value, "1")) {
        if (!strcmp(key, "Vulkan")) s->vulkan = value[0] == '1';
        else s->dlss = value[0] == '1';
      } else valid = false;
    } else if (!strcmp(key, "EnhancedRenderer")) {
      if (!strcmp(value, "0") || !strcmp(value, "1")) s->enhanced = value[0] == '1';
      else valid = false;
    } else if (!strcmp(key, "PresentationEnabled")) {
      has_fps_toggle = true;
      if (!strcmp(value, "0") || !strcmp(value, "1")) s->fps_enabled = value[0] == '1';
      else valid = false;
    } else if (!strcmp(key, "BSDeluxe")) {
      if (!strcmp(value, "0") || !strcmp(value, "1")) s->bs_deluxe = value[0] == '1';
      else valid = false;
    } else if (!strcmp(key, "Aspect")) {
      if (!FzeroParseAspect(value, &s->aspect)) valid = false;
    } else if (!strcmp(key, "PresentationFPS")) {
      unsigned fps;
      if (!strcmp(value, "Auto")) s->fps = 0;
      else if (sscanf(value, "%u%c", &fps, &tail) == 1 && FzeroValidFps(fps)) s->fps = fps;
      else valid = false;
    }
  }
  if (ferror(f)) valid = false;
  fclose(f);
  if (!has_fps_toggle) s->fps_enabled = s->enhanced; /* migrate combined checkpoint mod */
  return valid;
}

bool FzeroVideoSave(const FzeroVideoSettings *s, const char *path) {
  char temporary[1024];
  if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary)) return false;
  FILE *f = fopen(temporary, "w");
  if (!f) return false;
  bool ok = fprintf(f, "[FZeroVideo]\nEnhancedRenderer=%d\nAspect=%s\nPresentationEnabled=%d\nPresentationFPS=%u\nBSDeluxe=%d\nVulkan=%d\nDLSS5=%d\n",
                    s->enhanced, FzeroAspectName(s->aspect), s->fps_enabled, s->fps, s->bs_deluxe, s->vulkan, s->dlss) > 0;
  if (fclose(f)) ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(temporary, path) == 0;
#endif
  }
  if (!ok) remove(temporary);
  return ok;
}

void FzeroClockReset(FzeroClock *c, double now, double hz) {
  *c = (FzeroClock){.next_simulation = now, .next_presentation = now,
                    .presentation_hz = isfinite(hz) && hz > 0 ? hz : 60};
}
bool FzeroClockSimulationDue(const FzeroClock *c, double now) {
  return now >= c->next_simulation;
}
void FzeroClockSimulationDone(FzeroClock *c) {
  c->next_simulation += 1.0 / FZERO_SIMULATION_HZ;
  ++c->simulation_frames;
}
bool FzeroClockPresentationDue(const FzeroClock *c, double now) {
  return now >= c->next_presentation;
}
void FzeroClockPresentationDone(FzeroClock *c, double now) {
  double period = 1.0 / c->presentation_hz;
  double overdue = fmax(0, now - c->next_presentation);
  uint64_t missed = (uint64_t)floor(overdue / period);
  c->missed_presentations += missed;
  c->next_presentation += (missed + 1) * period;
  ++c->presentations;
}
double FzeroClockAlpha(const FzeroClock *c, double now) {
  return fmax(0, fmin(1, 1 - (c->next_simulation - now) * FZERO_SIMULATION_HZ));
}
double FzeroClockNextDeadline(const FzeroClock *c) {
  return fmin(c->next_simulation, c->next_presentation);
}
