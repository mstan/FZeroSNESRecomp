#pragma once
#include "recomp_launcher.h"
#ifdef __cplusplus
extern "C" {
#endif
const RecompLauncherCCustomContentProvider *
FzeroContentProvider(const char *mods, const char *helpers);
/* Join an in-flight import even if the launcher was closed. Returns whether
 * content changed. */
int FzeroContentShutdown(void);
#ifdef __cplusplus
}
#endif
