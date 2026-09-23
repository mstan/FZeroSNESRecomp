#pragma once
#include "fzero_course.h"
#include "content_pack.h"
const FzeroCourse *FzeroTracksCurrentCourse(void);
unsigned FzeroTracksRequiredFeatures(void);
void FzeroTracksRefreshCourse(void);
void FzeroTracksInstallHooks(void);
bool FzeroTracksRuntimeSelect(unsigned index);
unsigned FzeroTracksRuntimeCount(void);
const CpCup *FzeroTracksRuntimeCup(unsigned index, const CpPack **pack);
void FzeroTracksOverlay(uint32_t *pixels, unsigned width, unsigned height, size_t pitch);
void FzeroTracksFlush(void);
void FzeroTracksMenuReset(void);
unsigned FzeroTracksMenuIndex(void);
bool FzeroTracksMenuVisible(void);
bool FzeroTracksClassSelected(void);
void FzeroTracksSavesInit(void);
void FzeroTracksSavesFinish(void);
bool FzeroTracksRecordsSelect(const uint8_t *key);
size_t FzeroTracksSaveStateSize(void);
struct SaveLoadInfo;
void FzeroTracksSaveState(struct SaveLoadInfo *sli, bool load);
unsigned FzeroTracksCurrentCupSize(void);
