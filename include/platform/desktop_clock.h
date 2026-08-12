#ifndef GUARD_PLATFORM_DESKTOP_CLOCK_H
#define GUARD_PLATFORM_DESKTOP_CLOCK_H

#include "gba/types.h"
#include "siirtc.h"

typedef bool32 (*PlatformClockProvider)(struct SiiRtcInfo *rtc);

void Platform_ClockInit(void);
void Platform_ClockSetProvider(PlatformClockProvider provider);
void Platform_ClockResetProvider(void);
bool32 Platform_ClockCopyState(void *dest, u32 size);
bool32 Platform_ClockRestoreState(const void *source, u32 size);

#endif
