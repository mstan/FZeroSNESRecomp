#pragma once
#include "fzero_video.h"
#include "recomp_launcher.h"
const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings,
                                                   const char *path, bool bundled_music);
