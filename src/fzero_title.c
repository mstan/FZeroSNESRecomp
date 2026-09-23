#include "fzero_title.h"
#include "content_pack.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* F-Zero 55 retains the original title's sprite layout and DMA descriptors at
 * $03:9965. Its title tiles live at $0C:EC00-FFFF and palette at $0F:C2E0-C35F.
 * Apply the bundled artwork-only IPS to a disposable copy of the stock input,
 * then retain only those resources. No donor code or MSU data can be installed.
 * The fixed, reviewed loader layout is shared by the retail and BS engines. */
static uint8_t graphics[0x1480], identity[32];
static bool active;

void FzeroTitleReset(void) { active = false; }

bool FzeroTitlePrepare(const uint8_t *stock, size_t size, const char *path,
                       char *error, size_t error_size) {
    active = false;
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, error_size, "Missing title artwork patch");
        return false;
    }
    uint8_t patch[0x10000];
    size_t length = fread(patch, 1, sizeof(patch), file);
    bool ok = !ferror(file) && feof(file);
    fclose(file);
    if (!ok) {
        snprintf(error, error_size, "Cannot read title artwork patch (64 KiB limit)");
        return false;
    }
    uint8_t *donor = NULL;
    size_t donor_size = 0;
    if (!cp_patch_apply(stock, size, patch, length, &donor, &donor_size, error, error_size))
        return false;
    if (donor_size < 0x7c360) {
        free(donor);
        snprintf(error, error_size, "Incomplete title artwork");
        return false;
    }
    memcpy(graphics, donor + 0x66c00, 0x1400);
    memcpy(graphics + 0x1400, donor + 0x7c2e0, 0x80);
    free(donor);
    sha256_compute(graphics, sizeof(graphics), identity);
    active = true;
    return true;
}

const uint8_t *FzeroTitleHash(void) { return active ? identity : NULL; }

void FzeroTitleLoad(uint16_t vram[0x8000], uint8_t palette[0x80]) {
    if (!active) return;
    /* Word destination, byte offset into the title tiles, byte count.
     * Same order as the native DMA loop (last descriptor first). */
    static const uint16_t transfers[][3] = {
        {0x4000,0x000,0x180}, {0x4100,0x200,0x180}, {0x4200,0x400,0x180},
        {0x4300,0x600,0x180}, {0x4400,0x800,0x180}, {0x4500,0xa00,0x180},
        {0x4600,0x180,0x080}, {0x4700,0x380,0x080}, {0x4640,0x580,0x080},
        {0x4740,0x780,0x080}, {0x4680,0x980,0x080}, {0x4780,0xb80,0x080},
        {0x5200,0xc00,0x540}, {0x5500,0x1200,0x140},
        {0x5060,0x1140,0x0c0}, {0x5160,0x1340,0x0c0}
    };
    for (unsigned i = sizeof(transfers)/sizeof(*transfers); i-- > 0;) {
        const uint16_t *t = transfers[i];
        for (unsigned j = 0; j < t[2]; j += 2)
            vram[t[0] + j/2] = graphics[t[1]+j] | (uint16_t)graphics[t[1]+j+1] << 8;
    }
    memcpy(palette, graphics + 0x1400, 0x80);
}
