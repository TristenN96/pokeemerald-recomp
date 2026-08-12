#ifndef GUARD_PLATFORM_DESKTOP_FILESYSTEM_H
#define GUARD_PLATFORM_DESKTOP_FILESYSTEM_H

#include <stdio.h>

#include "gba/types.h"

FILE *Platform_FileOpen(const char *path, const char *mode);
bool32 Platform_FileReplace(const char *source, const char *destination);
bool32 Platform_DirectoryEnsure(const char *path);
bool32 Platform_FileRemove(const char *path);
bool32 Platform_DirectoryRemove(const char *path);

#endif
