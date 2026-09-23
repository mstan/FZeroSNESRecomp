/*
 * Headless F-Zero bring-up / soak host.
 *
 * No SDL, no window, no launcher: it boots the recompiled game, runs a fixed
 * number of frames and reports whether the run showed the activity a real boot
 * produces (WRAM churn, a non-uniform framebuffer that keeps changing, and
 * audible DSP output). Used for standup validation and for harvesting the
 * interpreter-tier coverage profile that feeds the next regeneration.
 */

#include "fzero_runtime.h"
#include "fzero_deluxe.h"
#include "fzero_gameplay.h"
#include "fzero_vehicles.h"
#include "fzero_tracks.h"
#include "fzero_course_runtime.h"
#include "fzero_msu.h"
#include "fzero_state_mode.h"
#include "fzero_replay.h"

#include "audio_trace.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "sha256.h"
#include "snes/apu.h"
#include "snes/cart.h"
#include "snes/dsp.h"
#include "snes/ppu.h"
#include "snes/snes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* F-Zero (USA), 512 KiB LoROM, headerless. */
static const uint8_t kFzeroSha256[32] = {
    0xbf, 0x16, 0xc3, 0xc8, 0x67, 0xc5, 0x8e, 0x2a,
    0xb0, 0x61, 0xc7, 0x0d, 0xe9, 0x29, 0x5b, 0x69,
    0x30, 0xd6, 0x3f, 0x29, 0xf8, 0x1c, 0xc9, 0x86,
    0xf5, 0xec, 0xae, 0x03, 0xe0, 0xad, 0x18, 0xd2,
};

enum { kFzeroRomSize = 0x80000u, kMaxInputSpans = 128 };

typedef struct AttractStats {
  uint64_t logic_hash;
  uint64_t video_hash;
  uint64_t logic_changes;
  uint64_t video_changes;
  uint64_t video_active_frames;
  uint64_t audio_active_frames;
  uint32_t audio_peak;
  uint64_t audio_underruns;
} AttractStats;

typedef struct WavWriter {
  FILE *stream;
  uint32_t data_bytes;
} WavWriter;

typedef struct InputSpan {
  long first;
  long last;
  uint32_t mask;
} InputSpan;

void headless_install_exception_filter(void);

static uint64_t fnv1a_update(uint64_t hash, const void *data, size_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static uint64_t logic_hash(void) {
  uint64_t hash = UINT64_C(14695981039346656037);
  hash = fnv1a_update(hash, g_ram, sizeof(g_ram));
  if (g_snes->cart->ram && g_snes->cart->ramSize)
    hash = fnv1a_update(hash, g_snes->cart->ram, g_snes->cart->ramSize);
  return hash;
}

static uint8_t *read_rom(const char *path, size_t *size_out,
                         uint8_t hash[32]) {
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
  sha256_compute(rom, *size_out, hash);
  return rom;
}

static void print_hash(const uint8_t hash[32]) {
  for (unsigned i = 0; i < 32; i++) fprintf(stderr, "%02x", hash[i]);
}

static void write_u16(FILE *stream, uint16_t value) {
  fputc(value & 0xff, stream);
  fputc(value >> 8, stream);
}

static void write_u32(FILE *stream, uint32_t value) {
  write_u16(stream, value & 0xffff);
  write_u16(stream, value >> 16);
}

static int wav_open(WavWriter *writer, const char *path) {
  memset(writer, 0, sizeof(*writer));
  if (!path || !path[0]) return 1;
  writer->stream = fopen(path, "wb");
  if (!writer->stream) return 0;
  fwrite("RIFF", 1, 4, writer->stream);
  write_u32(writer->stream, 0);
  fwrite("WAVEfmt ", 1, 8, writer->stream);
  write_u32(writer->stream, 16);
  write_u16(writer->stream, 1);
  write_u16(writer->stream, 2);
  write_u32(writer->stream, 32040);
  write_u32(writer->stream, 32040 * 4);
  write_u16(writer->stream, 4);
  write_u16(writer->stream, 16);
  fwrite("data", 1, 4, writer->stream);
  write_u32(writer->stream, 0);
  return ferror(writer->stream) == 0;
}

static int wav_append(WavWriter *writer, const int16_t *audio, int frames) {
  if (!writer->stream) return 1;
  size_t bytes = (size_t)frames * 2u * sizeof(audio[0]);
  if (writer->data_bytes > UINT32_MAX - bytes) return 0;
  if (fwrite(audio, 1, bytes, writer->stream) != bytes) return 0;
  writer->data_bytes += (uint32_t)bytes;
  return 1;
}

static int wav_close(WavWriter *writer) {
  if (!writer->stream) return 1;
  fseek(writer->stream, 4, SEEK_SET);
  write_u32(writer->stream, writer->data_bytes + 36u);
  fseek(writer->stream, 40, SEEK_SET);
  write_u32(writer->stream, writer->data_bytes);
  int ok = ferror(writer->stream) == 0 && fclose(writer->stream) == 0;
  writer->stream = NULL;
  return ok;
}

static int write_ppm(const char *path, const uint8_t *pixels, int width) {
  if (!path || !path[0]) return 1;
  FILE *stream = fopen(path, "wb");
  if (!stream) return 0;
  fprintf(stream, "P6\n%d 224\n255\n", width);
  for (size_t i = 0; i < (size_t)width * 224u; i++) {
    uint8_t rgb[3] = {pixels[i * 4u + 2u], pixels[i * 4u + 1u],
                      pixels[i * 4u]};
    if (fwrite(rgb, 1, sizeof(rgb), stream) != sizeof(rgb)) {
      fclose(stream);
      return 0;
    }
  }
  return fclose(stream) == 0;
}

static int write_wram_dump(const char *path) {
  if (!path || !path[0]) return 1;
  FILE *stream = fopen(path, "wb");
  if (!stream) return 0;
  int ok = fwrite(g_ram, 1, sizeof(g_ram), stream) == sizeof(g_ram);
  if (fclose(stream) != 0) ok = 0;
  return ok;
}

static void collect_video(AttractStats *stats, const uint8_t *pixels,
                          long frame, int width) {
  uint64_t hash = fnv1a_update(UINT64_C(14695981039346656037), pixels,
                               (size_t)width * 224u * 4u);
  if (frame && hash != stats->video_hash) stats->video_changes++;
  stats->video_hash = hash;
  const uint32_t *words = (const uint32_t *)pixels;
  uint32_t first = words[0] & 0xffffffu;
  for (size_t i = 1; i < (size_t)width * 224u; i++) {
    if ((words[i] & 0xffffffu) != first) {
      stats->video_active_frames++;
      break;
    }
  }
}

static void collect_audio(AttractStats *stats, const int16_t *audio,
                          int frames) {
  int active = 0;
  for (int i = 0; i < frames * 2; i++) {
    int64_t value = audio[i];
    uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
    if (magnitude) active = 1;
    if (magnitude > stats->audio_peak) stats->audio_peak = magnitude;
  }
  if (active) stats->audio_active_frames++;
}

static int parse_input_script(InputSpan spans[kMaxInputSpans],
                              size_t *count_out) {
  const char *cursor = getenv("SNESRECOMP_INPUT_SCRIPT");
  *count_out = 0;
  if (!cursor || !cursor[0]) return 1;
  while (*cursor) {
    while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') cursor++;
    if (!*cursor) break;
    if (*count_out >= kMaxInputSpans) return 0;
    char *end = NULL;
    long first = strtol(cursor, &end, 0);
    if (end == cursor || first < 0) return 0;
    cursor = end;
    long last = first;
    if (*cursor == '-') {
      cursor++;
      last = strtol(cursor, &end, 0);
      if (end == cursor || last < first) return 0;
      cursor = end;
    }
    if (*cursor++ != ':') return 0;
    unsigned long mask = strtoul(cursor, &end, 0);
    if (end == cursor || mask > 0x0fffu) return 0;
    cursor = end;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor && *cursor != ',') return 0;
    spans[*count_out].first = first;
    spans[*count_out].last = last;
    spans[*count_out].mask = (uint32_t)mask;
    (*count_out)++;
  }
  return 1;
}

static uint32_t scripted_input(const InputSpan *spans, size_t count,
                               long frame) {
  uint32_t input = 0;
  for (size_t i = 0; i < count; i++)
    if (frame >= spans[i].first && frame <= spans[i].last)
      input |= spans[i].mask;
  return input;
}

int main(int argc, char **argv) {
  /* Preserve the last guest-frame diagnostic if a soak terminates abnormally
   * while stderr is redirected to a qualification log. */
  setvbuf(stderr, NULL, _IONBF, 0);
  headless_install_exception_filter();
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: FZeroSNESRecompHeadless <fzero.sfc> [frames]\n");
    return 2;
  }
  long frame_limit = argc == 3 ? strtol(argv[2], NULL, 10) : 3600;
  if (frame_limit < 1 || frame_limit > 1000000) {
    fprintf(stderr, "frames must be between 1 and 1000000\n");
    return 2;
  }

  size_t rom_size = 0;
  uint8_t rom_hash[32];
  uint8_t *rom = read_rom(argv[1], &rom_size, rom_hash);
  if (!rom) {
    fprintf(stderr, "unable to read ROM: %s\n", argv[1]);
    return 2;
  }
  if (rom_size != kFzeroRomSize ||
      memcmp(rom_hash, kFzeroSha256, sizeof(rom_hash)) != 0) {
    fprintf(stderr, "unsupported F-Zero ROM (size=%zu sha256=", rom_size);
    print_hash(rom_hash);
    fputs(")\n", stderr);
    free(rom);
    return 2;
  }

  const char *track_root = getenv("FZERO_TRACK_PACKS");
  if (!FzeroTracksInit(track_root ? track_root : "mods/track-packs",
#ifdef FZERO_HAS_DELUXE
                       true
#else
                       false
#endif
                       )) { fprintf(stderr, "%s\n", FzeroTracksError()); free(rom); return 2; }
  const char *deluxe_data = getenv("FZERO_DELUXE_DATA");
  FzeroGameplayHeadless(deluxe_data && *deluxe_data);
  if (!FzeroTracksPrepare(&rom, &rom_size, deluxe_data && *deluxe_data, deluxe_data)) {
    fprintf(stderr, "[content] %s %s\n", FzeroTracksError(), FzeroDeluxeError());
    free(rom);
    return 2;
  }
  if (!FzeroTracksActive() && !FzeroMsuPrepare(&rom, &rom_size, getenv("SNESRECOMP_MSU1"), argv[1])) {
    fprintf(stderr, "[fzero-msu1] %s\n", FzeroMsuError());
    free(rom);
    return 2;
  }
  RtlRegisterGame(FzeroGameInfo());
  if (!SnesInit(rom, (int)rom_size)) {
    fputs("failed to initialize the F-Zero cartridge\n", stderr);
    free(rom);
    return 3;
  }
  const char *save_root = getenv("SNESRECOMP_SAVE_ROOT");
  if (save_root && save_root[0]) RtlSetSaveRoot(save_root);
  if (!FzeroTracksSelectSaveRoot()) {
    fprintf(stderr, "%s\n", FzeroDeluxeError());
    free(rom);
    return 3;
  }
  if (!FzeroMsuSelectSaveRoot()) {
    fprintf(stderr, "%s\n", FzeroMsuError());
    free(rom);
    return 3;
  }
  RtlReadSram();
  /* Reproduce a reported transition from a private, mode-checked snapshot. */
  const char *initial_state = getenv("FZERO_STATE_LOAD");
  if (initial_state && *initial_state) {
    if (!FzeroStateFileAcceptable(initial_state) || !RtlLoadSnapshot(initial_state)) {
      fprintf(stderr, "unable to load compatible state: %s\n", initial_state);
      free(rom);
      return 3;
    }
  }

  InputSpan input_spans[kMaxInputSpans];
  size_t input_span_count = 0;
  if (!parse_input_script(input_spans, &input_span_count)) {
    fputs("invalid SNESRECOMP_INPUT_SCRIPT; expected FIRST[-LAST]:MASK "
          "entries\n",
          stderr);
    free(rom);
    return 2;
  }
  if (!FzeroReplayConfigure(NULL, getenv("FZERO_VIEWPORT_SCRIPT"))) {
    fputs("invalid FZERO_VIEWPORT_SCRIPT\n", stderr);
    free(rom); return 2;
  }
  FzeroVideoSettings replay_video;
  FzeroVideoStock(&replay_video); /* headless baseline is stock; FZERO_ASPECT opts in */
  const char *initial_aspect = getenv("FZERO_ASPECT");
  if (initial_aspect && FzeroParseAspect(initial_aspect, &replay_video.aspect)) replay_video.enhanced = true;

  int frame_width = FzeroFrameWidth();
  int drawable_width = 768, drawable_height = 576;
  FzeroSetViewport(FzeroCalculateViewport(&replay_video, drawable_width, drawable_height));
  frame_width = FzeroFrameWidth();
  static uint8_t pixels[FZERO_MAX_WIDTH * 224u * 4u];
  int16_t audio[600 * 2];
  FzeroBeginDrawing(pixels, (size_t)frame_width * 4u);

  AttractStats stats = {0};
  WavWriter wav;
  if (!wav_open(&wav, getenv("SNESRECOMP_WAV"))) {
    fputs("unable to open WAV capture\n", stderr);
    free(rom);
    return 4;
  }
  double audio_accumulator = 0.0;
  /* Private validation: replay ten frames across an actual disk snapshot. */
  const char *lifecycle = getenv("FZERO_LIFECYCLE_TEST");
  static uint8_t replay_expected[0x20000];
  uint64_t replay_master = 0;
  const char *progress_test = getenv("FZERO_LIBRARY_PROGRESS_TEST");
  long next_completed_result = 1450;


  for (long frame = 0; frame < frame_limit; frame++) {
    /* Private integration check: inject a completed results state, then let
     * the unmodified GP transition routine advance/load/finish the cup.
     * This tests queue boundaries, not driving or finish-line detection. */
    if(progress_test && FzeroTracksActive() && frame>=next_completed_result &&
       g_ram[0x54]==2 && g_ram[0x55]==3) {
      fprintf(stderr,"library-progress: completed result ordinal=%u at frame=%ld\n",g_ram[0x53],frame);
      g_ram[0x54]=3;g_ram[0x55]=1;g_ram[0x56]=4;g_ram[0x60]=0;
      next_completed_result=frame+1000;
    }

    if (lifecycle && frame == 1500) {
      RtlEnsureSaveDir();
      char path[1024]; RtlSaveSlotPath(11, path, sizeof(path));
      if (!RtlSaveSnapshot(path)) { fputs("lifecycle: save failed\n", stderr); return 8; }
      for (long n = frame; n < frame + 10; ++n) {
        (void)RtlRunFrame(scripted_input(input_spans, input_span_count, n));
        if (g_fail || !FzeroLastLleResult()) return 8;
        FzeroDrawPpuFrame();
      }
      memcpy(replay_expected, g_ram, sizeof(replay_expected));
      replay_master = g_cpu.master_cycles;
      if (getenv("FZERO_VEHICLE_CROSS_STATE") && FzeroVehicleCount()>4) {
        g_ram[0x14dff]=FzeroVehicleSelected()>=4?0:4;
        FzeroVehiclesLoaded();
        fputs("lifecycle: switched vehicle cohort before restoring snapshot\n",stderr);
      }
      if (!RtlLoadSnapshot(path)) { fputs("lifecycle: load failed\n", stderr); return 8; }
      /* Compare identical replay paths. The normal host loop drains audio;
       * the speculative pass above does not, so comparing against that loop
       * also measured audio-service scheduling instead of snapshot fidelity. */
      for (long n = frame; n < frame + 10; ++n) {
        (void)RtlRunFrame(scripted_input(input_spans, input_span_count, n));
        if (g_fail || !FzeroLastLleResult()) return 8;
        FzeroDrawPpuFrame();
      }
      if (memcmp(replay_expected, g_ram, sizeof(replay_expected)) || replay_master != g_cpu.master_cycles) {
        fputs("lifecycle: resimulation differs after load\n", stderr);
        int reported = 0;
        for (size_t i = 0; i < sizeof(replay_expected) && reported < 12; ++i)
          if (replay_expected[i] != g_ram[i]) {
            fprintf(stderr, "  RAM %05zx: expected %02x got %02x\n", i, replay_expected[i], g_ram[i]);
            ++reported;
          }
        fprintf(stderr, "  master: expected %llu got %llu\n",
                (unsigned long long)replay_master, (unsigned long long)g_cpu.master_cycles);
        return 8;
      }
      fputs("lifecycle: save/load ten-frame resimulation identical (RAM and master clock)\n", stderr);
      if (!RtlLoadSnapshot(path)) return 8;
    }
    if (lifecycle && frame == 1800) {
      uint64_t before_reset = g_cpu.master_cycles;
      FzeroTracksSavesFinish();
      RtlReset(1); FzeroGameInfo()->session_reset();
      FzeroSetViewport(FzeroCalculateViewport(&replay_video, drawable_width, drawable_height));
      FzeroBeginDrawing(pixels, (size_t)frame_width * 4u);
      if (g_cpu.master_cycles != before_reset) return 8;
      fputs("lifecycle: soft reset, SRAM retained\n", stderr);
    }
    if (FzeroReplayViewport((unsigned)frame, &replay_video)) {
      FzeroReplayWindow((unsigned)frame, &drawable_width, &drawable_height);
      FzeroViewport viewport = FzeroCalculateViewport(&replay_video, drawable_width, drawable_height);
      FzeroSetViewport(viewport);
      frame_width = viewport.width;
      FzeroBeginDrawing(pixels, (size_t)frame_width * 4u);
      fprintf(stderr, "[fzero-viewport] frame=%ld width=%d\n", frame, frame_width);
    }
    (void)RtlRunFrame(scripted_input(input_spans, input_span_count, frame));
    if (getenv("FZERO_SCENE_TRACE"))
      fprintf(stderr, "scene %ld state=%02x,%02x,%02x training=%02x scenery=%02x sound=%02x,%02x,%02x,%02x,%02x msu=%02x,%02x,%02x,%02x brightness=%02x\n",
              frame, g_ram[0x54], g_ram[0x55], g_ram[0x56], g_ram[0x58], g_ram[0x81],
              g_ram[0x45], g_ram[0x46], g_ram[0x47], g_ram[0x48], g_ram[0x49],
              g_ram[0x180], g_ram[0x181], g_ram[0x182], g_ram[0x183], g_snes->ppu->inidisp);
    if (g_fail || !FzeroLastLleResult()) {
      fprintf(stderr, "fzero_native: runtime failure frame=%ld pc=$%06x bus_fault=%d execution=%d state=%02x,%02x,%02x car=%02x\n",
              frame, (unsigned)FzeroResumePc(), g_fail, FzeroLastLleResult(),
              g_ram[0x54], g_ram[0x55], g_ram[0x56], g_ram[0x52]);
      write_wram_dump(getenv("SNESRECOMP_WRAM_DUMP"));
      wav_close(&wav);
      free(rom);
      return 5;
    }

    uint64_t next_logic = logic_hash();
    if (frame && next_logic != stats.logic_hash) stats.logic_changes++;
    stats.logic_hash = next_logic;

    FzeroDrawPpuFrame();
    collect_video(&stats, pixels, frame, frame_width);

    audio_accumulator += 32040.0 / 60.098811862;
    int audio_frames = (int)audio_accumulator;
    audio_accumulator -= audio_frames;
    memset(audio, 0, sizeof(audio));
    RtlRenderAudio(audio, audio_frames, 2);
    collect_audio(&stats, audio, audio_frames);
    if (!wav_append(&wav, audio, audio_frames)) {
      fputs("unable to write WAV capture\n", stderr);
      wav_close(&wav);
      free(rom);
      return 7;
    }
  }

  int output_ok = wav_close(&wav) &&
                  write_ppm(getenv("SNESRECOMP_FRAME_DUMP"), pixels,
                            frame_width) &&
                  write_wram_dump(getenv("SNESRECOMP_WRAM_DUMP"));
  uint32_t audio_samples = g_snes->apu->dsp->sampleWrite;
  AudioTraceStats audio_stats;
  audio_trace_get_stats(&audio_stats);
  stats.audio_underruns = audio_stats.output_underflows;

  int qualified =
      frame_limit < 600 ||
      (stats.logic_changes >= (uint64_t)(frame_limit / 20) &&
       stats.video_active_frames >= (uint64_t)(frame_limit / 4) &&
       stats.video_changes >= (uint64_t)(frame_limit / 600) &&
       stats.audio_active_frames >= (uint64_t)(frame_limit / 10) &&
       stats.audio_peak > 0 && audio_samples > 0);

  fprintf(stderr,
          "fzero_native: %s frames=%ld resume=%06x master=%llu "
          "logic_changes=%llu video_active=%llu video_changes=%llu "
          "audio_samples=%u audio_active=%llu audio_peak=%u "
          "audio_underruns=%llu\n",
          qualified && output_ok ? "PASS" : "FAIL", frame_limit,
          (unsigned)FzeroResumePc(), (unsigned long long)g_cpu.master_cycles,
          (unsigned long long)stats.logic_changes,
          (unsigned long long)stats.video_active_frames,
          (unsigned long long)stats.video_changes, audio_samples,
          (unsigned long long)stats.audio_active_frames, stats.audio_peak,
          (unsigned long long)stats.audio_underruns);
  FzeroTracksSavesFinish();
  if (getenv("FZERO_RULE_PROBE")) {
    extern bool FzeroRulesProbe(void);
    if (!FzeroRulesProbe()) { free(rom); return 9; }
  }
  free(rom);
  return qualified && output_ok ? 0 : 8;
}
