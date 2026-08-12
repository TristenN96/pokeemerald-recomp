#ifndef GUARD_PLATFORM_DESKTOP_AUDIO_H
#define GUARD_PLATFORM_DESKTOP_AUDIO_H

#include "gba/types.h"

bool32 Platform_AudioInit(u32 sampleRate);
void Platform_AudioSetPaused(bool32 paused);
void Platform_AudioClearQueue(void);
void Platform_AudioShutdown(void);

#endif
