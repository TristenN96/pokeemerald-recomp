#ifndef GUARD_PLATFORM_DESKTOP_SCHEDULER_H
#define GUARD_PLATFORM_DESKTOP_SCHEDULER_H

#include "gba/types.h"

bool32 Platform_SchedulerInit(void);
bool32 Platform_SchedulerFrameAvailable(void);
bool32 Platform_SchedulerWaitForFrame(u32 timeoutMs);
void Platform_SchedulerCompleteFrame(void);
void Platform_SchedulerSetSpeed(u8 speed);
u8 Platform_SchedulerGetSpeed(void);
double Platform_SchedulerGetTimeScale(void);
bool32 Platform_SchedulerAudioFrameDue(void);
u64 Platform_SchedulerGetFrameCounter(void);
void Platform_SchedulerSetFrameCounter(u64 frame);
void Platform_SchedulerShutdown(void);

#endif
