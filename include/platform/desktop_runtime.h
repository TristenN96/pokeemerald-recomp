#ifndef GUARD_PLATFORM_DESKTOP_RUNTIME_H
#define GUARD_PLATFORM_DESKTOP_RUNTIME_H

#include <stdint.h>

#include "gba/types.h"

bool32 Platform_RuntimeGetImageRange(uintptr_t *start, uintptr_t *end);
bool32 Platform_RuntimeGetBssRange(uintptr_t *start, uintptr_t *end);
bool32 Platform_RuntimeAddressIsExecutable(uintptr_t address);
bool32 Platform_RuntimeAddressIsMapped(uintptr_t address);
bool32 Platform_RuntimeGetBuildId(const char *abiId, char *dest, u32 destSize);

#endif
