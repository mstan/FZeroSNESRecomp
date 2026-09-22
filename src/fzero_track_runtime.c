#include "fzero_tracks.h"
#include "fzero_deluxe.h"
#include "cpu_state.h"
#include "common_rtl.h"
#include "snes/interp_bridge.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const CpPack *active_pack;
static uint8_t identity[32];
bool FzeroTracksActive(void) { return active_pack != NULL; }
const uint8_t *FzeroTracksActiveHash(void) { return identity; }
const char *FzeroTracksActiveId(void) { return active_pack ? active_pack->id : ""; }

bool FzeroTracksPrepare(uint8_t **rom, size_t *size, bool deluxe, const char *deluxe_path) {
    active_pack = NULL; memset(identity, 0, sizeof(identity));
    const CpPack *p = NULL;
    if (*FzeroTracksSelection()) {
        if (!FzeroTracksValidate(*rom, *size) || !FzeroTracksSelected(&p)) return false;
        deluxe = !strcmp(p->adapter, "bs-deluxe");
    }
    if (!FzeroDeluxePrepare(rom, size, deluxe, deluxe_path)) return false;
    if (!p || !strcmp(p->adapter, "retail") || deluxe) return true;
    uint8_t *mapped = NULL; size_t mapped_size = 0; char error[256];
    if (!cp_pack_apply(p, *rom, *size, FzeroTracksPatch(p), &mapped, &mapped_size, error, sizeof(error))) {
        fprintf(stderr, "[track-pack] %s\n", error); return false;
    }
    /* AOT, including native interrupts, is valid only for its exact ROM.
     * An empty table keeps all dispatch paths on the interpreter floor. */
    cpu_select_interpreted_program();
    interp_bridge_set_scheduler_aot_policy(0);
    free(*rom); *rom = mapped; *size = mapped_size; active_pack = p;
    uint8_t key[CP_ID + 32] = {0};
    memcpy(key, p->id, strlen(p->id)); memcpy(key+CP_ID, p->target_hash, 32);
    sha256_compute(key, sizeof(key), identity);
    fprintf(stderr, "[track-pack] %s (%zu bytes); isolated interpreter program\n", p->name, *size);
    return true;
}
bool FzeroTracksSelectSaveRoot(void) {
    if (!active_pack) return FzeroDeluxeSelectSaveRoot();
    char hash[65], root[96]; cp_hash_format(identity, hash);
    if (snprintf(root, sizeof(root), "%s/%s", RtlSaveRoot(), hash) >= (int)sizeof(root)) {
        fprintf(stderr, "[track-pack] Save root is too long for the shared runtime; choose a shorter root\n");
        return false;
    }
    RtlEnsureSaveDir(); RtlSetSaveRoot(root); RtlEnsureSaveDir();
    fprintf(stderr, "[track-pack] saves: %s/save.srm\n", RtlSaveRoot());
    return true;
}
static uint8_t menu_pending, menu_pressed;
static bool cup_menu(const uint8_t *ram, const CpPack *p) {
    return !strcmp(p->adapter, "bs-deluxe")
        ? ram[0x54] == 1 && ram[0x55] == 1 && ram[0x56] == 2 && !ram[0x58]
        : ram[0x54] == 1 && ram[0x55] == 5 && !ram[0x58];
}
void FzeroTracksMenuState(uint8_t state[2], bool load) {
    if (load) { menu_pending = state[0]; menu_pressed = state[1]; }
    else { state[0] = menu_pending; state[1] = menu_pressed; }
}
void FzeroTracksMenuTick(uint8_t *ram, uint32_t previous_scene) {
    const CpPack *p = NULL; const CpCup *cup = FzeroTracksSelected(&p);
    if (!cup) return;
    uint32_t scene = ram[0x54] | (uint32_t)ram[0x55] << 8 | (uint32_t)ram[0x56] << 16;
    if (cup_menu(ram, p) && scene != previous_scene) {
        menu_pending = 1; menu_pressed = 0;
    }
}
uint16_t FzeroTracksMenuInput(uint16_t input, const uint8_t *ram) {
    if (!menu_pending) return input;
    const CpPack *p = NULL; const CpCup *cup = FzeroTracksSelected(&p);
    if (!cup || !cup_menu(ram, p)) { menu_pending = 0; return input; }
    unsigned address = !strcmp(p->adapter, "bs-deluxe") ? 0x90 : 0x5a;
    if (ram[address] == cup->slot) {
        menu_pending = 0;
        fprintf(stderr, "[track-library] highlighted %s/%s (slot %u)\n", p->id, cup->id, cup->slot);
        return input;
    }
    if (++menu_pending > 32) {
        menu_pending = 0;
        fprintf(stderr, "[track-library] Guest menu did not reach the selected cup; returning control\n");
        return input;
    }
    /* Navigate through the guest's own handler so cursor, tilemap palettes,
     * DMA queues and window masks update together. Two-frame pulses finish
     * within eight frames for the shipped cups; then control is untouched. */
    menu_pressed ^= 1;
    return menu_pressed ? 32 : 0;
}
