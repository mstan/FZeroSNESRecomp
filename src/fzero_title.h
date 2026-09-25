#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "content_pack.h"

typedef struct FzeroTitleScreen {
    char id[CP_ID], name[CP_NAME], patch[CP_PATH];
    bool hidden;
} FzeroTitleScreen;
/* Reviewed artwork registry, independent of enabled course packs. Original
 * is always first; hidden entries can restore legacy settings but aren't choices. */
void FzeroTitleCatalogInit(void);
const FzeroTitleScreen *FzeroTitleFind(const char *id);
const FzeroTitleScreen *FzeroTitleChoice(unsigned index);
unsigned FzeroTitleChoiceCount(void);

/* Presentation resources only, extracted using an artwork-only patch against
 * the verified stock input. The original/BS engine still runs the title. */
void FzeroTitleReset(void);
bool FzeroTitlePrepare(const uint8_t *stock, size_t size, const char *patch,
                       char *error, size_t error_size);
const uint8_t *FzeroTitleHash(void);
void FzeroTitleLoad(uint16_t vram[0x8000], uint8_t palette[0x80]);
