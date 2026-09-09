/*
 * F-Zero desktop host: recomp-ui launcher, SDL window/audio/input, save
 * states, SRAM and the shared crash-report path.
 */

#include "fzero_runtime.h"
#include "fzero_mods.h"
#include "fzero_deluxe.h"
#include "fzero_replay.h"
#include "fzero_dlss.h"

#include "common_rtl.h"
#include "cpu_trace.h"
#include "debug_server.h"
#include "host_report.h"
#include "keybinds.h"
#include "launcher_profile.h"
#include "recomp_launcher.h"
#include "sha256.h"
#include "snes/cart.h"
#include "snes/ppu.h"
#include "snes/snes.h"
#include "spc_player.h"
#include "types.h"

#include "third_party/gl_core/gl_core_3_1.h"
#include "glsl_shader.h"
#include "desktop/config.h"
#include "desktop/sdl_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  kFrameWidth = 256,
  kFrameHeight = 224,
  kBytesPerPixel = 4,
  kFzeroRomSize = 0x80000,
};

/* F-Zero (USA), 512 KiB LoROM, headerless. */
static const uint8_t kFzeroSha256[32] = {
    0xbf, 0x16, 0xc3, 0xc8, 0x67, 0xc5, 0x8e, 0x2a,
    0xb0, 0x61, 0xc7, 0x0d, 0xe9, 0x29, 0x5b, 0x69,
    0x30, 0xd6, 0x3f, 0x29, 0xf8, 0x1c, 0xc9, 0x86,
    0xf5, 0xec, 0xae, 0x03, 0xe0, 0xad, 0x18, 0xd2,
};
static const char *const kFzeroSha1[] = {
    "d3efd32b68f1fe37a82db9d9929b7ca7cc1a3af4",
};

/* Baked in by -DSNESRECOMP_BUILD_VERSION=<ver> (see CMakeLists.txt); local
 * builds report "dev". tools/make_release.ps1 verifies the stamp by scanning
 * the packaged binary for this literal, so it must reach .rodata. */
#ifndef SNESRECOMP_BUILD_VERSION
#define SNESRECOMP_BUILD_VERSION "dev"
#endif
static const char kBuildVersion[] = SNESRECOMP_BUILD_VERSION;

bool g_new_ppu = true;
static SDL_mutex *g_audio_mutex;
static const char kWindowTitle[] = "F-Zero";
static FzeroVideoSettings g_video;
Config g_config;
static bool g_reset_presentation_clock;
static const char *kVideoConfig = "fzero-video.ini";
static char video_config_path[1024];
static const char *const kFzeroAspectLabels[] = {
    "4:3",
    "16:9",
    "21:9",
    "32:9",
    "Fit to window",
};

static void spc_initialize(SpcPlayer *player) { (void)player; }
static void spc_upload(SpcPlayer *player, const uint8_t *data) {
  (void)player;
  (void)data;
}

static SpcPlayer g_sdl_spc_player = {
    .initialize = spc_initialize,
    .upload = spc_upload,
};

SpcPlayer *g_spc_player = &g_sdl_spc_player;

void NORETURN Die(const char *error) {
  fprintf(stderr, "fatal: %s\n", error ? error : "unknown error");
  fflush(stderr);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "F-Zero",
                           error ? error : "Unknown error", NULL);
  exit(EXIT_FAILURE);
}

#define GLSL_CODE(...) #__VA_ARGS__

typedef struct FzeroGlRenderer {
  SDL_Window *window;
  SDL_GLContext context;
  uint program;
  uint vao;
  uint vbo;
  GlTextureWithSize texture;
  GlslShader *shader;
} FzeroGlRenderer;

static void fzero_gl_prepare_window(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
}

static bool fzero_gl_compile(uint shader, const char *kind) {
  int ok = 0;
  char info[512] = {0};
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  glGetShaderInfoLog(shader, sizeof(info), NULL, info);
  if (ok != GL_TRUE) {
    fprintf(stderr, "[fzero-gl] %s shader compile failed:\n%s\n", kind, info);
    return false;
  }
  if (info[0]) fprintf(stderr, "[fzero-gl] %s shader compile log:\n%s\n", kind, info);
  return true;
}

static bool fzero_gl_link(uint program) {
  int ok = 0;
  char info[512] = {0};
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  glGetProgramInfoLog(program, sizeof(info), NULL, info);
  if (ok != GL_TRUE) {
    fprintf(stderr, "[fzero-gl] program link failed:\n%s\n", info);
    return false;
  }
  if (info[0]) fprintf(stderr, "[fzero-gl] program link log:\n%s\n", info);
  return true;
}

static bool fzero_gl_create_passthrough(FzeroGlRenderer *glr) {
  static const GLchar *vs_code = "#version 330 core\n" GLSL_CODE(
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main(void) {
      gl_Position = vec4(aPos, 1.0);
      TexCoord = aTexCoord;
    }
  );
  static const GLchar *fs_code = "#version 330 core\n" GLSL_CODE(
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D texture1;
    void main(void) {
      FragColor = texture(texture1, TexCoord);
    }
  );

  uint vs = glCreateShader(GL_VERTEX_SHADER);
  uint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(vs, 1, &vs_code, NULL);
  glShaderSource(fs, 1, &fs_code, NULL);
  bool ok = fzero_gl_compile(vs, "vertex") && fzero_gl_compile(fs, "fragment");
  if (ok) {
    glr->program = glCreateProgram();
    glAttachShader(glr->program, vs);
    glAttachShader(glr->program, fs);
    ok = fzero_gl_link(glr->program);
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!ok) return false;

  static const float vertices[] = {
      -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
      -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
       1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
       1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
  };
  glGenVertexArrays(1, &glr->vao);
  glGenBuffers(1, &glr->vbo);
  glBindVertexArray(glr->vao);
  glBindBuffer(GL_ARRAY_BUFFER, glr->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return true;
}

static bool fzero_gl_init(FzeroGlRenderer *glr, SDL_Window *window,
                          const char *shader_path) {
  memset(glr, 0, sizeof(*glr));
  glr->window = window;
  glr->context = SDL_GL_CreateContext(window);
  if (!glr->context) return false;
  SDL_GL_SetSwapInterval(1);
  ogl_LoadFunctions();
  if (!ogl_IsVersionGEQ(3, 3)) {
    fprintf(stderr, "[fzero-gl] OpenGL 3.3 is required for GLSL shaders\n");
    return false;
  }
  glGenTextures(1, &glr->texture.gl_texture);
  if (!fzero_gl_create_passthrough(glr)) return false;
  if (shader_path && shader_path[0]) {
    glr->shader = GlslShader_CreateFromFile(shader_path);
    if (!glr->shader) {
      fprintf(stderr, "[fzero-gl] Unable to load shader preset: %s\n", shader_path);
      return false;
    }
  }
  return true;
}

static void fzero_gl_render(FzeroGlRenderer *glr, const uint8_t *pixels,
                            int logical_width, FzeroViewport viewport,
                            int drawable_width, int drawable_height) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, glr->texture.gl_texture);
  if (glr->texture.width == logical_width && glr->texture.height == kFrameHeight) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, logical_width, kFrameHeight,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
  } else {
    glr->texture.width = (uint16)logical_width;
    glr->texture.height = (uint16)kFrameHeight;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logical_width, kFrameHeight, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
  }

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  FzeroRect rect = FzeroDestination(viewport, drawable_width, drawable_height);
  int viewport_y = drawable_height - rect.y - rect.h;
  if (glr->shader) {
    glBindVertexArray(glr->vao);
    GlslShader_Render(glr->shader, &glr->texture, rect.x, viewport_y, rect.w,
                      rect.h);
    glBindVertexArray(0);
  } else {
    int filter = g_config.linear_filtering ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glViewport(rect.x, viewport_y, rect.w, rect.h);
    glUseProgram(glr->program);
    glUniform1i(glGetUniformLocation(glr->program, "texture1"), 0);
    glBindVertexArray(glr->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
  }
  SDL_GL_SwapWindow(glr->window);
}

static void fzero_gl_destroy(FzeroGlRenderer *glr) {
  if (glr->shader) GlslShader_Destroy(glr->shader);
  glDeleteTextures(1, &glr->texture.gl_texture);
  glDeleteProgram(glr->program);
  glDeleteBuffers(1, &glr->vbo);
  glDeleteVertexArrays(1, &glr->vao);
  if (glr->context) SDL_GL_DestroyContext(glr->context);
  memset(glr, 0, sizeof(*glr));
}

void RtlApuLock(void) {
  if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
}

void RtlApuUnlock(void) {
  if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
}

static uint8_t *read_rom(const char *path, size_t *size_out) {
  FILE *stream = fopen(path, "rb");
  if (!stream) return NULL;
  if (fseek(stream, 0, SEEK_END) != 0) {
    fclose(stream);
    return NULL;
  }
  long length = ftell(stream);
  if (length <= 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return NULL;
  }
  uint8_t *rom = (uint8_t *)malloc((size_t)length);
  if (!rom || fread(rom, 1, (size_t)length, stream) != (size_t)length) {
    free(rom);
    fclose(stream);
    return NULL;
  }
  fclose(stream);

  size_t skip = (size_t)length % 1024u == 512u ? 512u : 0u;
  *size_out = (size_t)length - skip;
  if (skip) memmove(rom, rom + skip, *size_out);
  return rom;
}

static int verify_rom(const uint8_t *rom, size_t size) {
  uint8_t actual[32];
  if (size != (size_t)kFzeroRomSize) return 0;
  sha256_compute(rom, size, actual);
  return memcmp(actual, kFzeroSha256, sizeof(actual)) == 0;
}

static int resolve_rom(int argc, char **argv, char *path, size_t path_size,
                       RecompLauncherCSettings *settings) {
  memset(settings, 0, sizeof(*settings));
  settings->window_scale = 3;
  settings->enable_audio = 1;
  settings->audio_freq = 32040;
  settings->volume = 100;
  settings->player_src[0] = 1;
  settings->deadzone[0] = 25;
  settings->aspect_index = (int)g_video.aspect;
  {
    const char *shader_override = getenv("FZERO_SHADER");
    if (shader_override && shader_override[0])
      snprintf(settings->shader_path, sizeof(settings->shader_path), "%s",
               shader_override);
  }
  /* The built-in mod owns native presentation settings. */
  settings->adaptive_view = 0;
  settings->widescreen_hud = 0;

  if (argc > 1) {
    snprintf(path, path_size, "%s", argv[1]);
    return 1;
  }

  RecompLauncherCGameInfo game;
  memset(&game, 0, sizeof(game));
  launcher_profile_apply("snes", &game);
  game.name = "F-Zero";
  game.region = "(USA)";
  game.known_sha1_hex = kFzeroSha1;
  game.num_known_sha1 = 1;
  game.known_sha256 = &kFzeroSha256;
  game.num_known_sha256 = 1;
  game.num_players = 1;
  game.sram_path = "saves/fzero.srm";
  game.widescreen_supported = 0;
  game.adaptive_view_supported = 0;
  game.aspect_labels = kFzeroAspectLabels;
  game.num_aspect_labels = FZERO_ASPECT_COUNT;
  game.aspect_setting_label = "Aspect ratio";
  game.aspect_setting_help =
      "Used for stock presentation. The built-in Presentation mod's aspect "
      "option overrides this when that mod is enabled.";
  game.has_shader = 1;
  game.mods = FzeroModsProvider(&g_video, kVideoConfig);
  game.rom_cache_path = "rom.cfg";

  char initial_rom[1024] = {0};
  char assets_dir[1024] = ".";
  if (argv[0] && argv[0][0]) {
    snprintf(assets_dir, sizeof(assets_dir), "%s", argv[0]);
    char *slash = strrchr(assets_dir, '/');
    char *backslash = strrchr(assets_dir, '\\');
    char *separator = slash > backslash ? slash : backslash;
    if (separator)
      *separator = '\0';
    else
      snprintf(assets_dir, sizeof(assets_dir), "%s", ".");
  }
  FILE *probe = fopen("fzero.sfc", "rb");
  if (probe) {
    fclose(probe);
    snprintf(initial_rom, sizeof(initial_rom), "%s", "fzero.sfc");
  }

  int action =
      recomp_launcher_run_window("F-Zero \xE2\x80\x94 Launcher", settings,
                                 &game, assets_dir, initial_rom, path,
                                 path_size);
  if (action == 1) return 0;
  if (action == 0 && path[0]) return 1;
  if (initial_rom[0]) {
    snprintf(path, path_size, "%s", initial_rom);
    return 1;
  }
  return -1;
}

static FzeroAspect launcher_aspect(int index) {
  if (index < 0 || index >= FZERO_ASPECT_COUNT) return FZERO_ASPECT_STOCK;
  return (FzeroAspect)index;
}

static uint32_t keyboard_input(void) {
  /* SDL3 returns const bool*, SDL2 const Uint8*; the shim normalizes both. */
  const uint8_t *keys = snesrecomp_sdl_get_keyboard_state();
  /* keybinds_read_player() is the engine's shared keybinds.ini reader, so the
   * launcher's Controls page and the game agree on the bindings. Its bitmask
   * layout (keybinds.h) is bit0=R,1=L,2=X,3=A,4=Right,5=Left,6=Down,7=Up,
   * 8=Start,9=Select,10=Y,11=B; this host's own input word packs B=bit0,
   * Y=bit1,Select=bit2,Start=bit3,Up=bit4,Down=bit5,Left=bit6,Right=bit7,
   * A=bit8,X=bit9,L=bit10,R=bit11 (the convention controller_input() ORs
   * into). Map by button identity rather than relying on the fact that it
   * happens to be a straight bit reversal. */
  static const uint8_t kKbBitForInputBit[12] = {
      11, /* input bit0  B      <- keybinds bit11 B      */
      10, /* input bit1  Y      <- keybinds bit10 Y      */
      9,  /* input bit2  Select <- keybinds bit9  Select */
      8,  /* input bit3  Start  <- keybinds bit8  Start  */
      7,  /* input bit4  Up     <- keybinds bit7  Up     */
      6,  /* input bit5  Down   <- keybinds bit6  Down   */
      5,  /* input bit6  Left   <- keybinds bit5  Left   */
      4,  /* input bit7  Right  <- keybinds bit4  Right  */
      3,  /* input bit8  A      <- keybinds bit3  A      */
      2,  /* input bit9  X      <- keybinds bit2  X      */
      1,  /* input bit10 L      <- keybinds bit1  L      */
      0,  /* input bit11 R      <- keybinds bit0  R      */
  };
  uint16_t kb = keybinds_read_player(keys, 1); /* F-Zero is single-player */
  uint32_t input = 0;
  for (int i = 0; i < 12; i++)
    if ((kb >> kKbBitForInputBit[i]) & 1) input |= (1u << i);
  return input;
}

static uint32_t controller_input(SDL_GameController *pad) {
  if (!pad) return 0;
  uint32_t input = 0;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
    input |= 0x0001u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
    input |= 0x0002u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))
    input |= 0x0004u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
    input |= 0x0008u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
    input |= 0x0010u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
    input |= 0x0020u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
    input |= 0x0040u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
    input |= 0x0080u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
    input |= 0x0100u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y))
    input |= 0x0200u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
    input |= 0x0400u;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
    input |= 0x0800u;

  Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
  Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
  if (x < -12000) input |= 0x0040u;
  if (x > 12000) input |= 0x0080u;
  if (y < -12000) input |= 0x0010u;
  if (y > 12000) input |= 0x0020u;
  return input;
}

static void fill_audio(Uint8 *stream, int len) {
  if (!g_snes || len < 4) {
    SDL_memset(stream, 0, (size_t)len);
    return;
  }
  RtlRenderAudio((int16_t *)stream, len / 4, 2);
}

#if SNESRECOMP_SDL3
/* SDL3 replaced the pull callback with an SDL_AudioStream the app pushes into.
 * The stream sizes each pull itself, so render into a scratch buffer grown on
 * demand and hand the whole block over. */
static SDL_AudioStream *g_audio_stream;
static Uint8 *g_audio_scratch;
static size_t g_audio_scratch_size;

static void SDLCALL audio_stream_callback(void *userdata,
                                          SDL_AudioStream *stream,
                                          int additional_amount,
                                          int total_amount) {
  (void)userdata;
  (void)total_amount;
  if (additional_amount <= 0) return;

  /* Keep a cushion queued rather than feeding exactly what was asked for:
   * SDL3 pulls in whatever chunk the backend wants (~10 ms), and with no
   * reserve any hitch in the APU underruns immediately and is audible as
   * static. Top the queue back up to the historical 1024-frame depth; the
   * surplus is not lost, the stream keeps it for the next pull. */
  const int frame_bytes = 2 * (int)sizeof(int16_t); /* stereo S16 */
  const int target_queued = 1024 * frame_bytes;     /* 32 ms @ 32040 Hz */
  int queued = SDL_GetAudioStreamQueued(stream);
  if (queued < 0) queued = 0;
  int want_bytes = additional_amount;
  if (target_queued - queued > want_bytes) want_bytes = target_queued - queued;
  /* Byte counts "might be slightly overestimated" and nothing promises frame
   * alignment; a partial frame would permanently swap L/R. */
  want_bytes = ((want_bytes + frame_bytes - 1) / frame_bytes) * frame_bytes;

  if ((size_t)want_bytes > g_audio_scratch_size) {
    Uint8 *resized = (Uint8 *)realloc(g_audio_scratch, (size_t)want_bytes);
    if (!resized) return;
    g_audio_scratch = resized;
    g_audio_scratch_size = (size_t)want_bytes;
  }
  fill_audio(g_audio_scratch, want_bytes);
  SDL_PutAudioStreamData(stream, g_audio_scratch, want_bytes);
}
#else
static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
  (void)userdata;
  fill_audio(stream, len);
}
#endif

static void wait_until(double deadline) {
  double frequency = (double)SDL_GetPerformanceFrequency();
  double target = deadline * frequency;
  double now = (double)SDL_GetPerformanceCounter();
  while (now < target) {
    double remaining_ms = (target - now) * 1000.0 / frequency;
    if (remaining_ms > 1.5)
      SDL_Delay((Uint32)(remaining_ms - 0.5));
    else
      SDL_Delay(0);
    now = (double)SDL_GetPerformanceCounter();
  }
}

static double monotonic_seconds(void) {
  return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

static double display_refresh(SDL_Window *window) {
#if SNESRECOMP_SDL3
  const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
  return mode ? mode->refresh_rate : 0;
#else
  SDL_DisplayMode mode;
  return SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode) == 0 ? mode.refresh_rate : 0;
#endif
}

static int write_frame_bmp(const char *path, const uint8_t *pixels, int width,
                           int height) {
  if (!path || !path[0]) return 1;
  FILE *stream = fopen(path, "wb");
  if (!stream) return 0;
  uint32_t image_size = (uint32_t)width * (uint32_t)height * 4u;
  uint32_t pixel_offset = 54;
  uint32_t file_size = pixel_offset + image_size;
  uint8_t header[54] = {'B', 'M'};
  uint32_t dib_size = 40;
  int32_t bmp_width = width;
  int32_t bmp_height = -height;
  uint16_t planes = 1;
  uint16_t bits_per_pixel = 32;
  memcpy(header + 2, &file_size, 4);
  memcpy(header + 10, &pixel_offset, 4);
  memcpy(header + 14, &dib_size, 4);
  memcpy(header + 18, &bmp_width, 4);
  memcpy(header + 22, &bmp_height, 4);
  memcpy(header + 26, &planes, 2);
  memcpy(header + 28, &bits_per_pixel, 2);
  memcpy(header + 34, &image_size, 4);
  int ok = fwrite(header, 1, sizeof(header), stream) == sizeof(header) &&
           fwrite(pixels, 1, image_size, stream) == image_size;
  if (fclose(stream) != 0) ok = 0;
  return ok;
}

/* Uint64 deadlines: SDL3's SDL_GetTicks returns Uint64 and SDL_TICKS_PASSED is
 * gone, so compare absolute deadlines directly. Widening is harmless under
 * SDL2, where SDL_GetTicks returns Uint32. */
static void set_state_feedback(SDL_Window *window, const char *operation,
                               int slot, int ok, Uint64 *until) {
  char title[192];
  snprintf(title, sizeof(title), "%s - State %s %s (slot %d)", kWindowTitle,
           operation, ok ? "succeeded" : "failed", slot + 1);
  SDL_SetWindowTitle(window, title);
  *until = (Uint64)SDL_GetTicks() + 2500u;
}

static void perform_state_action(SDL_Window *window, int save, int slot,
                                 Uint64 *feedback_until) {
  char path[128];
  if (save) RtlEnsureSaveDir();
  RtlSaveSlotPath(slot, path, sizeof(path));
  int ok = save ? RtlSaveSnapshot(path) : RtlLoadSnapshot(path);
  if (!save && ok) g_reset_presentation_clock = true;
  set_state_feedback(window, save ? "save" : "load", slot, ok, feedback_until);
}

static void update_state_feedback(SDL_Window *window, Uint64 *feedback_until) {
  if (*feedback_until && (Uint64)SDL_GetTicks() >= *feedback_until) {
    SDL_SetWindowTitle(window, kWindowTitle);
    *feedback_until = 0;
  }
}

int main(int argc, char **argv) {
  SDL_SetMainReady();
  const char *config_override = getenv("FZERO_VIDEO_CONFIG");
  if (config_override && *config_override) kVideoConfig = config_override;
  else {
    const char *base = SDL_GetBasePath();
    if (base && snprintf(video_config_path, sizeof(video_config_path), "%sfzero-video.ini", base) < (int)sizeof(video_config_path))
      kVideoConfig = video_config_path;
#if !SNESRECOMP_SDL3
    SDL_free((void *)base);
#endif
  }
  if (!FzeroVideoLoad(&g_video, kVideoConfig))
    fprintf(stderr, "[fzero] Invalid video settings; invalid fields use defaults\n");
  if (!FzeroReplayConfigure(getenv("SNESRECOMP_INPUT_SCRIPT"), getenv("FZERO_VIEWPORT_SCRIPT"))) {
    fprintf(stderr, "[fzero] Invalid validation replay\n");
    return 2;
  }
  host_report_init(kWindowTitle, kBuildVersion);
  char rom_path[1024] = {0};
  RecompLauncherCSettings launcher_settings;
  int resolve_result =
      resolve_rom(argc, argv, rom_path, sizeof(rom_path), &launcher_settings);
  if (resolve_result <= 0) return resolve_result == 0 ? 0 : 2;
  if (!g_video.enhanced) {
    g_video.aspect = launcher_aspect(launcher_settings.aspect_index);
    if (!FzeroVideoSave(&g_video, kVideoConfig))
      fprintf(stderr, "[fzero] Unable to save video settings\n");
  }
  g_config.linear_filtering = launcher_settings.linear_filter != 0;
  size_t rom_size = 0;
  uint8_t *rom = read_rom(rom_path, &rom_size);
  if (!rom || !verify_rom(rom, rom_size)) {
    fprintf(stderr, "A verified F-Zero (USA) ROM is required: %s\n", rom_path);
    free(rom);
    return 2;
  }

  char deluxe_path[2048];
  const char *deluxe_override = getenv("FZERO_DELUXE_DATA");
  const char *deluxe_base = SDL_GetBasePath();
  snprintf(deluxe_path, sizeof(deluxe_path), "%smods/bs-deluxe.dat", deluxe_base ? deluxe_base : "");
#if !SNESRECOMP_SDL3
  SDL_free((void *)deluxe_base);
#endif
  if (!FzeroDeluxePrepare(&rom, &rom_size, g_video.bs_deluxe,
                          deluxe_override ? deluxe_override : deluxe_path)) {
    fprintf(stderr, "[bs-deluxe] %s\n", FzeroDeluxeError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "BS Deluxe", FzeroDeluxeError(), NULL);
    free(rom);
    return 2;
  }

  /* SDL_Init flipped to true-on-success in SDL3 and inverts silently in its
   * old `!= 0` form, so it must route through the shim. */
  if (!snesrecomp_sdl_init(SDL_INIT_VIDEO | SDL_INIT_AUDIO |
                           SDL_INIT_GAMECONTROLLER)) {
    fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    free(rom);
    return 3;
  }
  g_audio_mutex = SDL_CreateMutex();
  if (!g_audio_mutex) Die("Unable to create the audio mutex");

  /* Load (or generate) keybinds.ini next to the executable (cwd is
   * exe-anchored; the config-exe-anchoring convention shared by every SNES
   * recomp in this family). Must precede the first keyboard_input() call. */
  keybinds_init(NULL);

  RtlRegisterGame(FzeroGameInfo());
  if (!SnesInit(rom, (int)rom_size))
    Die("SNESRecomp rejected the F-Zero cartridge");
  cpu_trace_init();
  debug_server_set_ram(g_snes->ram, 0x20000);
  {
    int debug_port = 4385;
    const char *port_value = getenv("SNESRECOMP_DEBUG_PORT");
    if (port_value && port_value[0]) {
      long parsed = strtol(port_value, NULL, 0);
      if (parsed > 0 && parsed <= 65535) debug_port = (int)parsed;
    }
    if (debug_server_init(debug_port) == 0) {
#if SNESRECOMP_TRACE
      fprintf(stderr, "[fzero] Debug server ready on port %d\n", debug_port);
#endif
    }
  }
  const char *save_root = getenv("SNESRECOMP_SAVE_ROOT");
  if (save_root && *save_root) RtlSetSaveRoot(save_root);
  if (!FzeroDeluxeSelectSaveRoot()) Die(FzeroDeluxeError());
  RtlReadSram();

  /* SDL_WINDOW_ALLOW_HIGHDPI is one of the few old names SDL3 does NOT alias
   * in SDL_oldnames.h; it became SDL_WINDOW_HIGH_PIXEL_DENSITY. */
#if SNESRECOMP_SDL3
  const SDL_WindowFlags kHighDpiFlag = SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
  const Uint32 kHighDpiFlag = SDL_WINDOW_ALLOW_HIGHDPI;
#endif
  const char *output_method = getenv("FZERO_OUTPUT_METHOD");
  bool use_vulkan = output_method ? !strcmp(output_method, "Vulkan") : g_video.vulkan;
  bool use_gl_renderer = !use_vulkan && launcher_settings.shader_path[0] != 0;
  if (use_vulkan && launcher_settings.shader_path[0])
    fprintf(stderr, "[fzero] Vulkan selected; OpenGL shader preset is inactive\n");
  if (use_gl_renderer) fzero_gl_prepare_window();
  SDL_Window *window = snesrecomp_sdl_create_window(
      kWindowTitle, 768, 576,
      SDL_WINDOW_RESIZABLE | kHighDpiFlag |
          (use_gl_renderer ? SDL_WINDOW_OPENGL : 0));
  if (!window) Die("Unable to create the game window");
  if (launcher_settings.fullscreen)
    snesrecomp_sdl_set_fullscreen(window, true);
  FzeroGlRenderer gl_renderer;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
  if (use_gl_renderer) {
    if (!fzero_gl_init(&gl_renderer, window, launcher_settings.shader_path))
      Die("Unable to initialize the OpenGL shader renderer");
  } else {
    if (use_vulkan) {
#if SNESRECOMP_SDL3
      renderer = SDL_CreateRenderer(window, "vulkan");
      if (renderer) SDL_SetRenderVSync(renderer, 0);
#else
      Die("Vulkan presentation requires an SDL3 build");
#endif
    } else {
      renderer = snesrecomp_sdl_create_renderer(window, false, false);
      if (!renderer) renderer = snesrecomp_sdl_create_renderer(window, true, false);
    }
    if (!renderer) fprintf(stderr, "[fzero] Renderer initialization: %s\n", SDL_GetError());
    if (!renderer) Die("Unable to create the game renderer");
    fprintf(stderr, "[fzero] Presentation driver: %s\n", snesrecomp_sdl_renderer_name(renderer));
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, FZERO_MAX_WIDTH,
                                kFrameHeight);
    if (!texture) Die("Unable to create the game texture");
    /* Scale quality is per-texture in SDL3 (the SDL2 render hint is gone), and
     * the SNES framebuffer leaves alpha zero, so it must be marked opaque or
     * SDL3 blends the whole frame away and presents only the clear color. */
    snesrecomp_sdl_set_texture_linear(texture,
                                      launcher_settings.linear_filter != 0);
    snesrecomp_sdl_set_texture_opaque(texture);
  }

  static uint8_t pixels[FZERO_MAX_WIDTH * kFrameHeight * kBytesPerPixel];
  const char *dlss_env = getenv("FZERO_DLSS");
  bool dlss_enabled = use_vulkan && (dlss_env ? !strcmp(dlss_env, "1") : g_video.dlss) && FzeroDlssStart();
  SDL_Texture *dlss_texture = NULL;
  SDL_Texture *dlss_original_texture = NULL;
  bool dlss_compare = getenv("FZERO_DLSS_COMPARE") && !strcmp(getenv("FZERO_DLSS_COMPARE"), "1");
  int dlss_texture_width = 0, dlss_texture_height = 0;
  int drawable_width = 768, drawable_height = 576;
  if (use_gl_renderer)
    snesrecomp_sdl_get_drawable_size(window, &drawable_width, &drawable_height);
  else
    snesrecomp_sdl_get_render_output_size(renderer, &drawable_width, &drawable_height);
  FzeroViewport viewport = FzeroCalculateViewport(&g_video, drawable_width, drawable_height);
  FzeroSetViewport(viewport);
  FzeroSetDeferredPresentation(true);
  int logical_width = viewport.width;
  FzeroBeginDrawing(pixels, (size_t)logical_width * kBytesPerPixel);

  SDL_AudioSpec wanted = {0};
  wanted.freq = 32040;
  /* Native rate, so the conversion is a no-op - but state it rather than
   * leaning on the consumer's default. */
  RtlSetAudioOutputRate(32040);
  wanted.format = AUDIO_S16SYS;
  wanted.channels = 2;
  SDL_AudioDeviceID audio = 0;
#if SNESRECOMP_SDL3
  /* SDL_AudioSpec has no `samples`/`callback` in SDL3: the device is opened as
   * a stream and the callback is supplied separately. */
  g_audio_stream = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted, audio_stream_callback, NULL);
  if (g_audio_stream) audio = SDL_GetAudioStreamDevice(g_audio_stream);
#else
  SDL_AudioSpec obtained = {0};
  wanted.samples = 1024;
  wanted.callback = audio_callback;
  audio = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
#endif
  if (!audio) Die("Unable to open the audio device");
  snesrecomp_sdl_pause_audio_device(audio, launcher_settings.enable_audio == 0);

  SDL_GameController *pad = NULL;
#if SNESRECOMP_SDL3
  {
    /* SDL3 enumerates by instance ID rather than by index. */
    int njs = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&njs);
    for (int i = 0; i < njs; i++) {
      if (!SDL_IsGamepad(joysticks[i])) continue;
      pad = SDL_OpenGamepad(joysticks[i]);
      if (pad) break;
    }
    SDL_free(joysticks);
  }
#else
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i)) {
      pad = SDL_GameControllerOpen(i);
      if (pad) break;
    }
  }
#endif

  int running = 1;
  int paused = 0;
  Uint64 state_feedback_until = 0;
  long frames = 0;
  long auto_close_frames = 0;
  const char *auto_close = getenv("SNESRECOMP_AUTOCLOSE_FRAMES");
  if (auto_close) auto_close_frames = strtol(auto_close, NULL, 10);
  FzeroClock clock;
  double hz = g_video.fps_enabled ? FzeroPresentationHz(g_video.fps, display_refresh(window)) : FZERO_SIMULATION_HZ;
  FzeroClockReset(&clock, monotonic_seconds(), hz);
  bool suspended = false;
  double next_display_check = 0;
  uint64_t presentations = 0, missed_presentations = 0;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = 0;
      if (event.type == SDL_KEYDOWN && !event.key.repeat) {
        /* The keysym struct was flattened in SDL3; the shim macros pick the
         * right member for each major. */
        const SDL_Keycode key = SNESRECOMP_SDL_EVENT_KEY(event);
        const Uint16 mod = (Uint16)SNESRECOMP_SDL_EVENT_MOD(event);
        if ((mod & KMOD_CTRL) && key == SDLK_F8 && use_vulkan) {
          dlss_enabled = !dlss_enabled;
          if (dlss_enabled) dlss_enabled = FzeroDlssStart();
          else FzeroDlssStop();
          g_video.dlss = dlss_enabled;
          FzeroVideoSave(&g_video, kVideoConfig);
          FzeroDlssReset();
          fprintf(stderr, "[dlss] %s\n", dlss_enabled ? "enabled" : "disabled");
          continue;
        }
        if ((mod & KMOD_CTRL) && key == SDLK_F9 && use_vulkan) {
          dlss_compare = !dlss_compare;
          next_display_check = 0;
          continue;
        }
        if ((mod & KMOD_CTRL) && (key == SDLK_F6 || key == SDLK_F7)) {
          if (key == SDLK_F6) {
            if (!g_video.enhanced) { g_video.enhanced = true; g_video.aspect = FZERO_ASPECT_16_9; }
            else if (g_video.aspect == FZERO_ASPECT_FIT) g_video.enhanced = false;
            else g_video.aspect++;
          } else {
            g_video.fps_enabled = true;
            static const unsigned rates[] = {0,60,90,120,144,165,240,360};
            for (unsigned i = 0; i < sizeof(rates) / sizeof(*rates); ++i)
              if (g_video.fps == rates[i]) { g_video.fps = rates[(i + 1) % 8]; break; }
          }
          if (!FzeroVideoSave(&g_video, kVideoConfig)) fprintf(stderr, "[fzero] Unable to save video settings\n");
          next_display_check = 0;
          char title[192];
          snprintf(title, sizeof(title), "F-Zero - %s - %s%u FPS",
                   g_video.enhanced ? FzeroAspectName(g_video.aspect) : "4:3",
                   g_video.fps ? "" : "Auto / ", g_video.fps ? g_video.fps : (unsigned)display_refresh(window));
          SDL_SetWindowTitle(window, title);
          state_feedback_until = (Uint64)SDL_GetTicks() + 2500;
          continue;
        }
        if (key >= SDLK_F1 && key <= SDLK_F12) {
          int slot = (int)(key - SDLK_F1);
          perform_state_action(window, (mod & KMOD_SHIFT) != 0, slot,
                               &state_feedback_until);
          continue;
        }
        switch (key) {
          case SDLK_ESCAPE:
            running = 0;
            break;
          case SDLK_p:
            paused = !paused;
            break;
          case SDLK_r:
            if (mod & KMOD_CTRL) {
              RtlReset(1);
              FzeroGameInfo()->session_reset();
              FzeroSetViewport(viewport);
              g_reset_presentation_clock = true;
            }
            break;
          case SDLK_RETURN: {
            if (!(mod & KMOD_ALT)) break;
            Uint32 flags = (Uint32)SDL_GetWindowFlags(window);
            snesrecomp_sdl_set_fullscreen(
                window,
                (flags & SNESRECOMP_SDL_WINDOW_FULLSCREEN_DESKTOP) == 0);
            break;
          }
          default:
            break;
        }
      }
    }

    update_state_feedback(window, &state_feedback_until);
    {
      int slot = debug_server_consume_loadstate();
      if (slot >= 0)
        perform_state_action(window, 0, slot, &state_feedback_until);
      slot = debug_server_consume_savestate();
      if (slot >= 0)
        perform_state_action(window, 1, slot, &state_feedback_until);
    }
    double debug_wait_start = monotonic_seconds();
    debug_server_wait_if_paused();
    if (monotonic_seconds() - debug_wait_start > 0.1)
      g_reset_presentation_clock = true;
    if (!running) break;
    bool should_suspend = paused || (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
    if (should_suspend != suspended) {
      suspended = should_suspend;
      snesrecomp_sdl_pause_audio_device(audio, suspended || !launcher_settings.enable_audio);
      g_reset_presentation_clock = true;
    }
    if (suspended) { SDL_Delay(10); continue; }
    double now = monotonic_seconds();
    if (now >= next_display_check) {
      if (use_vulkan && !state_feedback_until) {
        char title[192];
        snprintf(title, sizeof(title), "F-Zero - Vulkan - DLSS5: %s%s",
                 FzeroDlssStatus(), dlss_enabled && dlss_compare ? " - Left: Original | Right: DLSS5" : "");
        SDL_SetWindowTitle(window, title);
      }
      double next_hz = g_video.fps_enabled ? FzeroPresentationHz(g_video.fps, display_refresh(window)) : FZERO_SIMULATION_HZ;
      if (next_hz != hz) {
        hz = next_hz;
        clock.presentation_hz = hz;
        clock.next_presentation = now;
      }
      next_display_check = now + 0.25;
    }
    if (g_reset_presentation_clock) {
      FzeroDlssReset();
      missed_presentations += clock.missed_presentations;
      FzeroClockReset(&clock, now, hz);
      g_reset_presentation_clock = false;
    }
    /* Viewport policy and input sampling change only at simulation boundaries. */
    for (unsigned batch = 0; batch < 4 && FzeroClockSimulationDue(&clock, now); ++batch) {
      FzeroReplayViewport((unsigned)frames, &g_video);
      int replay_width, replay_height;
      if (FzeroReplayWindow((unsigned)frames, &replay_width, &replay_height))
        SDL_SetWindowSize(window, replay_width, replay_height);
      if (use_gl_renderer)
        snesrecomp_sdl_get_drawable_size(window, &drawable_width, &drawable_height);
      else
        snesrecomp_sdl_get_render_output_size(renderer, &drawable_width, &drawable_height);
      FzeroViewport next = FzeroCalculateViewport(&g_video, drawable_width, drawable_height);
      if (next.width != viewport.width || next.aspect != viewport.aspect) {
        FzeroDlssReset();
        viewport = next;
        FzeroSetViewport(viewport);
        logical_width = viewport.width;
        FzeroBeginDrawing(pixels, (size_t)logical_width * kBytesPerPixel);
      }
      uint32_t input = keyboard_input() | controller_input(pad) |
                       debug_server_get_controller_inputs() | (1u << 30) |
                       debug_server_get_controller_active_mask();
      if (FzeroReplayHasInput()) input = FzeroReplayInput((unsigned)frames);
      (void)RtlRunFrame(input);
      if (g_fail || !FzeroLastLleResult()) {
        fprintf(stderr, "[fzero-failure] frame=%ld resume=$%06x bus_fault=%d execution=%d state=%02x,%02x,%02x car=%02x\n",
                frames, (unsigned)FzeroResumePc(), g_fail, FzeroLastLleResult(),
                g_ram[0x54], g_ram[0x55], g_ram[0x56], g_ram[0x52]);
        Die("F-Zero runtime execution failed");
      }
      FzeroDrawPpuFrame();
      frames++;
      FzeroClockSimulationDone(&clock);
      now = monotonic_seconds();
      if (auto_close_frames > 0 && frames >= auto_close_frames) { running = 0; break; }
    }

    if (FzeroClockPresentationDue(&clock, now)) {
      FzeroPresent(FzeroClockAlpha(&clock, now));
      if (use_gl_renderer) {
        fzero_gl_render(&gl_renderer, pixels, logical_width, viewport,
                        drawable_width, drawable_height);
      } else {
        SDL_Rect source = {0, 0, logical_width, kFrameHeight};
        SDL_Texture *present_texture = texture;
        SDL_UpdateTexture(texture, &source, pixels,
                          logical_width * kBytesPerPixel);
        if (dlss_enabled) {
          int nw = 0, nh = 0;
          const uint32_t *neural = FzeroDlssFrame((const uint32_t *)pixels, logical_width,
                                                 kFrameHeight, viewport.aspect, &nw, &nh);
          if (neural) {
            if (nw != dlss_texture_width || nh != dlss_texture_height) {
              SDL_DestroyTexture(dlss_texture);
              SDL_DestroyTexture(dlss_original_texture);
              dlss_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, nw, nh);
              dlss_original_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                         SDL_TEXTUREACCESS_STREAMING, nw, nh);
              dlss_texture_width = nw; dlss_texture_height = nh;
              if (dlss_texture) {
                snesrecomp_sdl_set_texture_opaque(dlss_texture);
                snesrecomp_sdl_set_texture_linear(dlss_texture, g_config.linear_filtering);
              }
              if (dlss_original_texture) {
                snesrecomp_sdl_set_texture_opaque(dlss_original_texture);
                snesrecomp_sdl_set_texture_linear(dlss_original_texture, g_config.linear_filtering);
              }
            }
            if (dlss_texture) {
              SDL_UpdateTexture(dlss_texture, NULL, neural, nw * 4);
              if (dlss_compare && dlss_original_texture)
                SDL_UpdateTexture(dlss_original_texture, NULL, FzeroDlssOriginal(), nw * 4);
              present_texture = dlss_texture;
              source = (SDL_Rect){0, 0, nw, nh};
            }
          }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        FzeroRect rect = FzeroDestination(viewport, drawable_width, drawable_height);
        SDL_Rect destination = {rect.x, rect.y, rect.w, rect.h};
        snesrecomp_sdl_render_texture(renderer, present_texture, &source, &destination);
        if (dlss_compare && dlss_enabled && present_texture == dlss_texture && dlss_original_texture) {
          SDL_Rect left_source = source;
          SDL_Rect left_destination = destination;
          left_source.w /= 2;
          left_destination.w = (int)((int64_t)destination.w * left_source.w / source.w);
          snesrecomp_sdl_render_texture(renderer, dlss_original_texture, &left_source, &left_destination);
        }
#if SNESRECOMP_SDL3
        const char *capture_path = getenv("FZERO_PRESENT_CAPTURE");
        const char *capture_frame = getenv("FZERO_PRESENT_CAPTURE_FRAME");
        static bool captured_present;
        if (!captured_present && capture_path && capture_frame && frames >= strtol(capture_frame, NULL, 10)) {
          SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
          if (surface) {
            SDL_SaveBMP(surface, capture_path);
            SDL_DestroySurface(surface);
          }
          captured_present = true;
        }
#endif
        SDL_RenderPresent(renderer);
      }
      FzeroClockPresentationDone(&clock, monotonic_seconds());
      ++presentations;
    }
    if (running) wait_until(FzeroClockNextDeadline(&clock));
  }
  fprintf(stderr, "[fzero-presentation] simulation=%ld presentations=%llu missed=%llu target_hz=%.3f\n",
          frames, (unsigned long long)presentations,
          (unsigned long long)(missed_presentations + clock.missed_presentations), hz);

  const char *frame_dump = getenv("SNESRECOMP_FRAME_BMP");
  if (!write_frame_bmp(frame_dump, pixels, logical_width, kFrameHeight))
    fprintf(stderr, "Unable to write frame dump: %s\n", frame_dump);
  const char *ram_dump = getenv("SNESRECOMP_WRAM_DUMP");
  if (ram_dump && ram_dump[0]) {
    FILE *dump = fopen(ram_dump, "wb");
    if (!dump || fwrite(g_ram, sizeof(g_ram), 1, dump) != 1)
      fprintf(stderr, "Unable to write WRAM capture\n");
    if (dump) fclose(dump);
  }
  RtlWriteSram();
  FzeroDlssStop();
  SDL_DestroyTexture(dlss_texture);
  SDL_DestroyTexture(dlss_original_texture);
  debug_server_shutdown();
  snesrecomp_sdl_pause_audio_device(audio, true);
#if SNESRECOMP_SDL3
  /* Destroying the stream closes the device it was opened against. */
  SDL_DestroyAudioStream(g_audio_stream);
  g_audio_stream = NULL;
  free(g_audio_scratch);
  g_audio_scratch = NULL;
  g_audio_scratch_size = 0;
#else
  SDL_CloseAudioDevice(audio);
#endif
  if (pad) SDL_GameControllerClose(pad);
  if (use_gl_renderer) {
    fzero_gl_destroy(&gl_renderer);
  } else {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
  }
  SDL_DestroyWindow(window);
  SDL_DestroyMutex(g_audio_mutex);
  g_audio_mutex = NULL;
  SDL_Quit();
  free(rom);
  return 0;
}
