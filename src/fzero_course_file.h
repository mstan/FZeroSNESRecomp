#pragma once
#include "fzero_course.h"

/* Portable, versioned resource stream; no executable bytes or struct padding.
 */
bool FzeroCourseFileWrite(const char *path, const FzeroCourse *course,
                          char *error, size_t cap);
bool FzeroCourseFileRead(const char *path, FzeroCourse *course, char *error,
                         size_t cap);
bool FzeroCourseValidate(const FzeroCourse *course, char *error, size_t cap);
void FzeroCourseHash(FzeroCourse *course);
