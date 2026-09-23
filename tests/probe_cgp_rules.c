/* Private-ROM semantic probes, linked only into the headless validator.
 * Execute the actual guest instructions and installed adapters at boundaries
 * where effects can be asserted without requiring a particular driving replay.
 */
#include "common_rtl.h"
#include "cpu_state.h"
#include "fzero_deluxe.h"
#include "fzero_gameplay.h"
#include "fzero_course_runtime.h"
#include "sha256.h"
#include "snes/interp_bridge.h"
#include <stdio.h>
#include <string.h>

#define CHECK(e)                                                               \
  do {                                                                         \
    if (!(e)) {                                                                \
      fprintf(stderr, "rules-probe:%d: %s\n", __LINE__, #e);                   \
      return false;                                                            \
    }                                                                          \
  } while (0)
static unsigned stopped;
static void stop(CpuState *cpu, uint32_t pc) {
  (void)cpu;
  stopped = pc;
  interp_bridge_pre_opcode_redirect(0x7e2000);
}
static void word(unsigned a, unsigned v) {
  g_ram[a] = (uint8_t)v;
  g_ram[a + 1] = (uint8_t)(v >> 8);
}
static unsigned read_word(unsigned a) {
  return g_ram[a] | (unsigned)g_ram[a + 1] << 8;
}
static CpuState state(unsigned p) {
  CpuState c = g_cpu;
  c.A = c.X = c.Y = c.D = c.DB = c.PB = 0;
  c.S = 0x1fff;
  c.P = (uint8_t)p;
  c.emulation = 0;
  cpu_p_to_mirrors(&c);
  return c;
}
static bool fragment(CpuState *c, unsigned start, unsigned end,
                     unsigned alternate, bool long_return) {
  stopped = 0;
  FzeroGameplayInstallHooks();
  FzeroTracksInstallHooks();
  if (end)
    interp_bridge_set_pre_opcode_hook(end, stop);
  if (alternate)
    interp_bridge_set_pre_opcode_hook(alternate, stop);
  g_ram[0x2000] = long_return ? 0x6b : 0x60;
  if (long_return)
    cpu_push_jsl_return_frame(c);
  else
    cpu_push_jsr_return_frame(c);
  CHECK(interp_bridge_run(c, start));
  CHECK(c->S == 0x1fff);
  CHECK(!end || stopped == end || (alternate && stopped == alternate));
  interp_bridge_set_pre_opcode_hook(0, NULL);
  return true;
}
bool FzeroRulesProbe(void) {
  static const DispatchEntry empty[1] = {{0}};
  cpu_select_program(empty, 0, NULL, 0);
  interp_bridge_set_pre_opcode_hook(0, NULL);
  if (FzeroTracksCurrentCourse()) {
    /* Exercise actual landing/recovery instruction boundaries for every tile
     * and both jump-state variants. Valid custom tiles above D0 must survive;
     * pits below D0 must remain fatal. Check both engine modules. */
    const FzeroCourse *course = FzeroTracksCurrentCourse();
    unsigned custom_safe = 0, custom_pit = 0;
    for (unsigned tile = 0; tile < 256; ++tile) {
      for (unsigned jumping = 0; jumping < 2; ++jumping) {
        CpuState c = state(0x10);
        unsigned flags = course->terrain[0x300 + tile];
        unsigned fatal = (flags & 0x80) || (!jumping && (flags & 0x20));
        g_ram[0xcd0] = (uint8_t)tile;
        g_ram[0xc00] = (uint8_t)flags;
        g_ram[0xd51] = (uint8_t)jumping;
        CHECK(fragment(&c, 0x009c9a, 0x009cc1, 0x009caf, false));
        CHECK(stopped == (fatal ? 0x009caf : 0x009cc1));
        custom_safe += tile >= 0xd0 && !fatal;
        custom_pit += tile < 0xd0 && (flags & 0x80);
      }
      for (unsigned deep = 0; deep < 2; ++deep) {
        CpuState c = state(0x30);
        g_ram[0xcd0] = (uint8_t)tile;
        g_ram[0xc01] = deep ? 0xff : 0;
        CHECK(fragment(&c, 0x00eb90, 0x00eb97, 0x00ebad, false));
        CHECK(stopped == (deep ? 0x00eb97 : 0x00ebad));
      }
    }
    for (unsigned tile = 0; tile < 256; ++tile) {
      unsigned flags = course->terrain[0x300 + tile];
      if (!(flags & 0x40) || (flags & 0x80)) continue;
      CpuState c = state(0x30);
      g_ram[0xcd0] = (uint8_t)tile;
      word(0xb20, 0);
      word(0xcc0, 0);
      CHECK(fragment(&c, 0x008e36, 0x008e28, 0, false));
      CHECK(read_word(0xb20) == 0x80 && read_word(0xc00) == 0x340);
    }
    fprintf(stderr, "rules-probe: course landings/recovery=1024 safe-high=%u low-pits=%u PASS\n",
            custom_safe, custom_pit);
  }
  if (FzeroRuleEnabled(FZERO_RULE_BOOST)) {
    static const unsigned drains[3][4] = {
        {23, 21, 19, 23}, {22, 21, 23, 20}, {22, 17, 22, 21}};
    unsigned profile = FzeroGameplaySettingsCurrent()->boost,
             bank = 0x33 + profile, drain = 0;
    const uint8_t pattern[] = {0xad, 0x51, 0x0d, 0x29, 0x38, 0xc9, 0x08};
    for (unsigned at = 0x8000; at < 0x8400; ++at) {
      unsigned n = 0;
      while (n < sizeof(pattern) &&
             cpu_read8(&g_cpu, (uint8_t)bank, (uint16_t)(at + n)) == pattern[n])
        ++n;
      if (n == sizeof(pattern)) {
        CHECK(!drain);
        drain = (bank << 16) | at;
      }
    }
    CHECK(drain);
    unsigned end = 0;
    for (unsigned at = drain; at < drain + 0x100; ++at)
      if (cpu_read8(&g_cpu, (uint8_t)bank, (uint16_t)at) == 0x22 &&
          cpu_read8(&g_cpu, (uint8_t)bank, (uint16_t)(at + 1)) == 0x91 &&
          cpu_read8(&g_cpu, (uint8_t)bank, (uint16_t)(at + 2)) == 0x87) {
        end = at;
        break;
      }
    CHECK(end);
    for (unsigned car = 0; car < (FzeroBsCars() ? 8u : 4u); ++car) {
      CpuState c = state(0x30);
      g_ram[0x52] = (uint8_t)car;
      g_ram[0x53] = 3;
      g_ram[0xd51] = 8;
      g_ram[0xd40] = 1;
      g_ram[0xee6] = 0;
      g_ram[0xcf4] = 8;
      word(0xc9, 1000);
      CHECK(fragment(&c, drain, end, 0, false));
      CHECK(read_word(0xc9) == 1000 - drains[profile][car & 3]);
      c = state(0x30);
      g_ram[0xcf3] = 3;
      CHECK(fragment(&c, 0x00b82f, 0x00b835, 0, false));
      CHECK(g_ram[0xcf4] == 150 && g_ram[0xcf3] == 2);
    }
  }
  if (FzeroRuleEnabled(FZERO_RULE_TUNING) &&
      !FzeroRuleEnabled(FZERO_RULE_LEGEND)) {
    uint8_t values[4 * 29 * 2], hash[32];
    unsigned n = 0;
    for (unsigned car = 0; car < 4; ++car)
      for (unsigned speed = 0; speed < 29; ++speed)
        for (unsigned actor = 0; actor <= 2; actor += 2) {
          CpuState c = state(0x30);
          c.X = (uint16_t)actor;
          c.A = (uint16_t)speed;
          g_ram[0x52] = (uint8_t)car;
          g_ram[0xd71 + actor] =
              (uint8_t)(FzeroDeluxeActive() ? car : car * 29);
          CHECK(fragment(&c, 0x0094ce, 0x0094d7, 0, false));
          CHECK(c.Y == actor);
          values[n++] = (uint8_t)c.A;
        }
    sha256_compute(values, sizeof(values), hash);
    fputs("rules-probe: acceleration=", stderr);
    for (unsigned i = 0; i < 32; ++i)
      fprintf(stderr, "%02x", hash[i]);
    fputc('\n', stderr);
  }
  if (FzeroRuleEnabled(FZERO_RULE_EXHAUST)) {
    uint8_t sprites[4 * 13 * 17], hash[32];
    unsigned n = 0;
    for (unsigned car = 0; car < 4; ++car)
      for (unsigned frame = 0; frame < 13; ++frame) {
        CpuState c = state(0x30);
        c.Y = (uint16_t)frame;
        g_ram[0x52] = (uint8_t)car;
        word(0x20, 0x2020);
        memset(g_ram + 0x2f0, 0, 16);
        g_ram[0x40f] = 0;
        unsigned entry =
            FzeroDeluxeActive() ? 0x1eadde
            : car ? cpu_read16(&c, 0, (uint16_t)(0xbe98 + 2 * (car - 1)))
                  : 0xc68c;
        CHECK(fragment(&c, entry, 0, 0, FzeroDeluxeActive()));
        memcpy(sprites + n, g_ram + 0x2f0, 16);
        n += 16;
        sprites[n++] = g_ram[0x40f];
      }
    sha256_compute(sprites, sizeof(sprites), hash);
    fputs("rules-probe: exhaust=", stderr);
    for (unsigned i = 0; i < 32; ++i)
      fprintf(stderr, "%02x", hash[i]);
    fputc('\n', stderr);
  }
  if (FzeroRuleEnabled(FZERO_RULE_LAPS)) {
    g_ram[0xad] = 42;
    const unsigned checkpoints[] = {0, 42, 20};
    for (unsigned i = 0; i < 3; ++i) {
      CpuState c = state(0x30);
      g_ram[0xd00] = (uint8_t)checkpoints[i];
      CHECK(fragment(&c, 0x009968, 0x00996d, 0x0099bc, false));
      CHECK(stopped == (i == 2 ? 0x0099bc : 0x00996d));
    }
    CpuState c = state(0x30);
    c.X = 2;
    CHECK(fragment(&c, 0x009968, 0x00996d, 0x0099bc, false));
    CHECK(stopped == 0x00996d);
  }
  if (FzeroRuleEnabled(FZERO_RULE_DASH)) {
    CpuState c = state(0x31);
    g_ram[0xbd1] = 50;
    g_ram[0x109d] = 20;
    g_ram[0xbe1] = 90;
    CHECK(fragment(&c, 0x009829, 0x00982e, 0, false));
    CHECK((c.A & 255) == 30);
    for (unsigned air = 0; air < 2; ++air) {
      c = state(0x30);
      g_ram[0xd51] = (uint8_t)(air * 128);
      g_ram[0xbe1] = 90;
      CHECK(fragment(&c, 0x009842, 0x009847, 0x00986c, false));
      CHECK(g_ram[0xbe1] == (air ? 90 : 20));
    }
  }
  if (FzeroRuleEnabled(FZERO_RULE_CPU_SPIN)) {
    CpuState c = state(0x10);
    c.Y = 2;
    word(0xb22, 0x800);
    word(0x15, 0);
    CHECK(fragment(&c, 0x00bd1a, 0x00bd1f, 0, false));
    CHECK(read_word(0xc22) == 0x8300 && c.m_flag);
  }
  if (FzeroRuleEnabled(FZERO_RULE_BOUNCE)) {
    CpuState c = state(0x30);
    c.A = 99;
    CHECK(fragment(&c, 0x00bc6d, 0x00bc72, 0, false));
    CHECK((c.A & 255) == 10);
  }
  if (FzeroRuleEnabled(FZERO_RULE_DMAG)) {
    CpuState c = state(0x30);
    g_ram[0xd50] = 8;
    g_ram[0xd51] = 0;
    CHECK(fragment(&c, 0x00b890, 0x00b8b1, 0, false));
    CHECK((c.A & 255) == 0x38);
    c = state(0x30);
    CHECK(fragment(&c, 0x00b8e6, 0x00b8f3, 0, false));
    CHECK((c.A & 255) == 0x50);
    c = state(0x30);
    word(0x17, 0);
    CHECK(fragment(&c, FzeroDeluxeActive() ? 0x1eac6d : 0x009292,
                   FzeroDeluxeActive() ? 0x1eacc6 : 0x0092f9, 0,
                   FzeroDeluxeActive()));
    CHECK(read_word(0x17) == 0x220);
  }
  if (FzeroRuleEnabled(FZERO_RULE_RAINBOW)) {
    for (unsigned rainbow = 0; rainbow < 2; ++rainbow) {
      CpuState c = state(0x30);
      g_ram[0xadf] = rainbow ? 255 : 0;
      g_ram[0x14] = 0x18;
      CHECK(fragment(&c, 0x0091e6, 0x0091eb, 0, false));
      CHECK(g_ram[0x14] == (rainbow ? 0x38 : 0x18));
    }
  }
  if (FzeroRuleEnabled(FZERO_RULE_LEGEND)) {
    for (unsigned rival = 0; rival < 256; ++rival) {
      CpuState c = state(0x30);
      c.X = (uint16_t)rival;
      CHECK(fragment(&c, 0x00d54c, 0x00d55d, 0, false));
      CHECK(g_ram[0x57] < 5);
    }
    for (unsigned level = 0; level < 5; ++level) {
      CpuState c = state(0x30);
      g_ram[0x57] = (uint8_t)level;
      g_ram[0xf3d] = 255;
      CHECK(fragment(&c, FzeroDeluxeActive() ? 0x1ec835 : 0x038841,
                     FzeroDeluxeActive() ? 0x1ec84a : 0x03884a, 0, false));
      CHECK(g_ram[0x59] == 7 - level && !g_ram[0xf3d] && g_ram[0x57] == level);
    }
  }
  if (FzeroDeluxeActive() && !FzeroBsTracks()) {
    for (unsigned track = 15; track < 25; ++track) {
      CpuState c = state(0);
      c.A = (uint16_t)track;
      CHECK(fragment(&c, 0x1eb805, 0, 0, false));
      CHECK(c._flag_Z);
    }
  }
  fprintf(stderr, "rules-probe: PASS (guest instructions, native return "
                  "stacks, conditional effects)\n");
  return true;
}
