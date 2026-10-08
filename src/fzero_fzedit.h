#pragma once
#include "fzero_course.h"
#ifdef __cplusplus
extern "C" {
#endif
bool FzeroFzeditRead(const char *pack_root, const char *path, FzeroCourse *out,
                     char *error, size_t cap);
bool FzeroFzeditReadCached(const char *pack_root, const char *path,
                         const char *cache_directory, FzeroCourse *out,
                         char *error, size_t cap);
#ifdef __cplusplus
}
#endif
