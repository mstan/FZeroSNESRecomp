#include "fzero_records.h"
#include "fzero_course_runtime.h"
#include "fzero_tracks.h"
#include "fzero_deluxe.h"
#include "fzero_gameplay.h"
#include "fzero_vehicles.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "snes/cart.h"
#include "snes/interp_bridge.h"
#include <stdio.h>
#include <string.h>
extern Snes *g_snes;

static bool scene(void) {
  return !g_ram[0x54] && g_ram[0x55] >= 2 && g_ram[0x55] <= (FzeroDeluxeActive() ? 3 : 5);
}
bool FzeroRecordsDetail(void) {
  return FzeroRecordsViewState() && scene() &&
      (FzeroDeluxeActive() ? g_ram[0x55] == 3 && g_ram[0x56] >= 2 && g_ram[0x56] <= 4 : g_ram[0x55] >= 4);
}
unsigned FzeroRecordsCup(void) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  return v ? v->page * 3 + v->selected / 5 : 0;
}
unsigned FzeroRecordsOrder(void) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  return v ? v->selected % 5 : 0;
}
static bool valid(unsigned slot) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  return v && slot < 15 && FzeroTracksRuntimeTrack(v->page * 3 + slot / 5, slot % 5);
}
static unsigned record_offset(unsigned slot) {
  static const unsigned starts[] = {5, 0xac, 0x153, 0x205, 0x2ac};
  return slot < 25 ? starts[slot / 5] + slot % 5 * 33 : 0x353 + (slot - 25) * 33;
}
static void page_records(void) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  uint8_t data[0x400], key[32];
  for (unsigned column = 0; column < 3; ++column) {
    unsigned index = v->page * 3 + column;
    const CpPack *p = NULL;
    const CpCup *cup = FzeroTracksRuntimeCup(index, &p);
    bool external = cup && FzeroTracksRecordKey(index,
        FzeroVehicleRecordIdentity(v->vehicle), v->practice, key);
    FzeroRecordsRead(external ? key : NULL, data);
    for (unsigned row = 0; row < 5; ++row) {
      uint8_t *dest = g_sram + record_offset(column * 5 + row);
      const CpTrack *track = FzeroTracksRuntimeTrack(index, row);
      if (track) {
        bool native = strcmp(p->adapter, "fzero-course-v1") != 0;
        unsigned source = native ? cup->slot * 5 + row : row;
        if (native && v->practice && FzeroDeluxeActive()) {
          if (source == 16) source = 25;
          if (source == 19) source = 26;
        }
        memcpy(dest, data + record_offset(source), 33);
      } else {
        for (unsigned i = 0; i < 11; ++i) memcpy(dest + i * 3, "\x09\x59\x99", 3);
      }
    }
    unsigned start = record_offset(column * 5), sum = 0;
    for (unsigned i = 0; i < 165; ++i) sum += g_sram[start + i];
    g_sram[start + 165] = (uint8_t)sum;
    g_sram[start + 166] = (uint8_t)(sum >> 8);
  }
  memcpy(g_ram + 0x14800, g_sram, 0x200);
  if (!valid(v->selected)) {
    v->selected = 0;
    while (v->selected < 15 && !valid(v->selected)) ++v->selected;
  }
  fprintf(stderr, "[records-browser] page=%u vehicle=%s mode=%s selected=%u\n",
      v->page, FzeroVehicleRecordName(v->vehicle), v->practice ? "practice" : "gp", v->selected);
}
void FzeroRecordsTick(void) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!scene()) {
    if (v) FzeroRecordsViewEnd();
    return;
  }
  if (!FzeroTracksActive()) return;
  if (!v) {
    if (!FzeroRecordsViewBegin()) return;
    v = FzeroRecordsViewState();
    v->previous_cup = (uint16_t)FzeroTracksMenuIndex();
    v->page = v->previous_cup / 3;
    v->selected = (v->previous_cup % 3) * 5;
    v->practice = g_ram[0x58] != 0;
    v->vehicle = (uint8_t)FzeroVehicleCount();
    const char *id = FzeroVehicleIdentity();
    for (unsigned i = 0; id && i < FzeroVehicleCount(); ++i)
      if (!strcmp(id, FzeroVehicleRecordIdentity(i))) v->vehicle = (uint8_t)i;
    page_records();
  }
}
static bool overview(void) {
  return scene() && g_ram[0x55] == 3 && (!FzeroDeluxeActive() || !g_ram[0x56]);
}
static void select_native(void) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!v) return;
  if (FzeroDeluxeActive()) {
    g_ram[0x53] = v->selected < 15 ? v->selected : 255;
    g_ram[0x14c88] = g_ram[0x14c89] = 0;
  } else g_ram[0xf2] = v->selected;
}
uint16_t FzeroRecordsInput(uint16_t input) {
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!v) return input;
  unsigned edge = input & ~v->input;
  unsigned direction = input & 0xf0;
  if (direction && direction == (v->input & 0xf0) && ++v->hold >= 24 && v->hold % 6 == 0)
    edge |= direction;
  if (direction != (v->input & 0xf0)) v->hold = 0;
  if (edge) v->label_tick = 0;
  else ++v->label_tick;
  v->input = input;
  if (overview()) {
    if (edge & (1024 | 2048 | 512 | 2 | 4)) {
      unsigned pages = (FzeroTracksRuntimeCount() + 2) / 3, cars = FzeroVehicleCount() + 1;
      if (edge & 1024) v->page = (v->page + pages - 1) % pages;
      if (edge & 2048) v->page = (v->page + 1) % pages;
      if (edge & 2) v->vehicle = (v->vehicle + cars - 1) % cars;
      if (edge & 512) v->vehicle = (v->vehicle + 1) % cars;
      if (edge & 4) v->practice ^= 1;
      page_records();
      /* Only the records and labels change. Keep the native frame in place
       * instead of replaying its fade/music setup on every page or car. */
    } else if (edge & 0xf0) {
      unsigned candidate = v->selected;
      if (edge & (64 | 128)) {
        candidate = candidate < 10 ? (candidate % 5 + 10) : (candidate == 15 ? 5 : candidate % 5);
        if (valid(candidate)) v->selected = (uint8_t)candidate;
        else v->selected = 15;
      } else {
        do { candidate = (candidate + ((edge & 16) ? 15 : 1)) % 16; }
        while (candidate != 15 && !valid(candidate));
        v->selected = (uint8_t)candidate;
      }
    }
    if (edge & 1) { v->selected = 15; input = 8; }
    select_native();
    return input & (8 | 256);
  }
  if (FzeroRecordsDetail()) {
    bool ready = FzeroDeluxeActive() ? g_ram[0x56] == 4 : g_ram[0x55] == 5;
    if (ready && (edge & 0xf0)) {
      unsigned candidate = v->selected;
      do { candidate = (candidate + ((edge & (16 | 64)) ? 14 : 1)) % 15; }
      while (!valid(candidate));
      v->selected = (uint8_t)candidate;
      select_native();
      if (FzeroDeluxeActive()) g_ram[0x56] = 3;
      else g_ram[0x55] = 4;
    }
    /* Deletion operates on native slots. This browser is a read-only view;
     * don't expose a delete command which could target a different context. */
    return (input & (8 | 256)) | ((edge & 1) ? 8 : 0);
  }
  return 0;
}
static void record_hook(CpuState *cpu, uint32_t pc) {
  FzeroRecordsTick();
  FzeroRecordsView *v = FzeroRecordsViewState();
  if (!v) {
    /* Same predicate as the gameplay hook this site replaces. */
    if (pc == 0x1eb805 && !FzeroBsTracks() && (cpu->A & 255) >= 15 && (cpu->A & 255) < 25) {
      cpu->_flag_Z = 1;
      interp_bridge_pre_opcode_redirect(0x1eb80a);
    }
    return;
  }
  switch (pc) {
  case 0x03822e: case 0x1eef87:
    select_native();
    /* Native completed-course indices refer to the raced context, not the
     * assembled three-cup page. Don't display them beneath another cup. */
    if (FzeroDeluxeActive()) memset(g_ram + 0x14cf0, 255, 10);
    break;
  case 0x1eefe7:
    cpu->A = v->selected < 15 ? v->selected : 0xffff;
    interp_bridge_pre_opcode_redirect(0x1eefea);
    break;
  case 0x038271: case 0x1ef024:
    select_native();
    break;
  case 0x1eb805:
    if (overview() || g_ram[0x55] == 2) {
      cpu->_flag_Z = !valid(cpu->A & 255);
      interp_bridge_pre_opcode_redirect(0x1eb80a);
    }
    break;
  case 0x1eeb1a: {
    /* Keep the native detail layout/renderer, but resolve resources from the
     * catalog. Its record slot stays virtual and never becomes a race slot. */
    const CpPack *p;
    const CpCup *cup = FzeroTracksRuntimeCup(FzeroRecordsCup(), &p);
    unsigned native = cup && strcmp(p->adapter, "fzero-course-v1") ? cup->slot * 5 + FzeroRecordsOrder() : 0;
    FzeroTracksRefreshCourse();
    const FzeroCourse *c = FzeroTracksCurrentCourse();
    if (c) {
      /* Keep the native detail-screen setup and sprite reservations. The
       * host overlay replaces its stock illustration with course scenery;
       * decoded course data also supplies the minimap and palette below. */
      for (unsigned i = 0; i < 15; ++i)
        if ((g_snes->cart->rom[0x16129 + i] & 15) == (c->setting & 15)) { native = i; break; }
      for (unsigned i = 0; i < 15; ++i)
        if (g_snes->cart->rom[0x16129 + i] == c->setting) { native = i; break; }
    }
    if (v->practice) {
      if (native == 16) native = 25;
      else if (native == 17) native = 26;
      else if (native == 19) native = 27;
      else if (native == 22) native = 28;
    }
    memcpy(g_ram + 0x14c00, g_snes->cart->rom + 0xf2000 + native * 64, 64);
    g_ram[0x14c25] = g_ram[0x14c27] = g_ram[0x14c28] = v->selected;
    g_ram[0x14c26] = 255; g_ram[0x14c29] = 0;
    FzeroTracksRefreshCourse();
    const CpTrack *track = FzeroTracksRuntimeTrack(FzeroRecordsCup(), FzeroRecordsOrder());
    fprintf(stderr, "[records-browser] detail %s/%s course=%s slot=%u\n", p->id, cup->id, track->id, v->selected);
    break;
  }
  case 0x1eec0b: {
    FzeroTracksRefreshCourse();
    const FzeroCourse *c = FzeroTracksCurrentCourse();
    if (c) {
      for (unsigned row = 0; row < 32; ++row) {
        memcpy(g_ram + 0x103b0 + row * 32, c->minimap + row * 16, 16);
        memset(g_ram + 0x103c0 + row * 32, 0, 16);
      }
      interp_bridge_pre_opcode_redirect(0x1eec45);
    }
    break;
  }
  case 0x1ee826: {
    const FzeroCourse *c = FzeroTracksCurrentCourse();
    if (c) memcpy(g_ram + 0x5c0, c->palette + 0xa0, 0x40);
    break;
  }
  }
}
void FzeroRecordsInstallHooks(void) {
  if (!FzeroTracksActive()) return;
  const uint32_t stock[] = {0x03822e, 0x038271};
  const uint32_t deluxe[] = {0x1eef87, 0x1eefe7, 0x1ef024, 0x1eb805, 0x1eeb1a, 0x1eec0b, 0x1ee826};
  const uint32_t *sites = FzeroDeluxeActive() ? deluxe : stock;
  unsigned count = FzeroDeluxeActive() ? sizeof(deluxe)/sizeof(*deluxe) : sizeof(stock)/sizeof(*stock);
  for (unsigned i = 0; i < count; ++i) interp_bridge_set_pre_opcode_hook(sites[i], record_hook);
}
