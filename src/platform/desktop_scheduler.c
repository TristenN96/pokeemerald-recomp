#ifdef PLATFORM_SDL2

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "global.h"
#include "platform/dma.h"
#include "platform/desktop_scheduler.h"

extern void AgbMain(void);
extern void (*const gIntrTable[])(void);

HOST_DATA static SDL_Thread *sMainLoopThread;
HOST_DATA static SDL_sem *sVBlankSemaphore;
HOST_DATA static SDL_atomic_t sFrameAvailable;
HOST_DATA static u8 sSpeed = 1;
HOST_DATA static u64 sFrameCounter;
HOST_DATA static u64 sAudioClockCounter;
HOST_DATA static double sAudioAccumulator;

static int RunAgbMain(void *data)
{
    (void)data;
    AgbMain();
    return 0;
}

bool32 Platform_SchedulerInit(void)
{
    sSpeed = 1;
    sFrameCounter = 0;
    sAudioClockCounter = SDL_GetPerformanceCounter();
    sAudioAccumulator = 0.0;
    SDL_AtomicSet(&sFrameAvailable, 0);
    sVBlankSemaphore = SDL_CreateSemaphore(0);
    if (sVBlankSemaphore == NULL)
        return FALSE;
    sMainLoopThread = SDL_CreateThread(RunAgbMain, "AgbMain", NULL);
    if (sMainLoopThread == NULL)
    {
        SDL_DestroySemaphore(sVBlankSemaphore);
        sVBlankSemaphore = NULL;
        return FALSE;
    }
    return TRUE;
}

bool32 Platform_SchedulerFrameAvailable(void)
{
    return sVBlankSemaphore != NULL && SDL_AtomicGet(&sFrameAvailable);
}

bool32 Platform_SchedulerWaitForFrame(u32 timeoutMs)
{
    u32 start = SDL_GetTicks();
    while (!Platform_SchedulerFrameAvailable())
    {
        if (timeoutMs != 0 && SDL_GetTicks() - start >= timeoutMs)
            return FALSE;
        SDL_Delay(0);
    }
    return TRUE;
}

void Platform_SchedulerCompleteFrame(void)
{
    if (sVBlankSemaphore == NULL || !SDL_AtomicGet(&sFrameAvailable))
        return;
    sFrameCounter++;
    SDL_AtomicSet(&sFrameAvailable, 0);
    REG_VCOUNT = 161;
    REG_DISPSTAT |= INTR_FLAG_VBLANK;
    RunDMAs(DMA_HBLANK);
#ifdef __ANDROID__
    if (REG_IE & INTR_FLAG_VBLANK)
#else
    if (REG_DISPSTAT & DISPSTAT_VBLANK_INTR)
#endif
        gIntrTable[4]();
    REG_DISPSTAT &= ~INTR_FLAG_VBLANK;
    SDL_SemPost(sVBlankSemaphore);
}

void Platform_SchedulerSetSpeed(u8 speed)
{
    sSpeed = speed;
}

u8 Platform_SchedulerGetSpeed(void)
{
    return sSpeed;
}

double Platform_SchedulerGetTimeScale(void)
{
    return sSpeed == 0 ? 0.0 : sSpeed;
}

bool32 Platform_SchedulerAudioFrameDue(void)
{
    const double audioFrameDuration = 1.0 / 60.0;
    u64 now = SDL_GetPerformanceCounter();
    u64 frequency = SDL_GetPerformanceFrequency();
    double delta;

    if (frequency == 0)
        return FALSE;
    delta = (double)(now - sAudioClockCounter) / (double)frequency;
    sAudioClockCounter = now;
    if (delta > 0.25)
        delta = audioFrameDuration;
    sAudioAccumulator += delta;
    if (sAudioAccumulator < audioFrameDuration)
        return FALSE;
    sAudioAccumulator -= audioFrameDuration;
    if (sAudioAccumulator >= audioFrameDuration)
        sAudioAccumulator = 0.0;
    return TRUE;
}

void VBlankIntrWait(void)
{
    if (sVBlankSemaphore == NULL)
        return;
    SDL_AtomicSet(&sFrameAvailable, 1);
    SDL_SemWait(sVBlankSemaphore);
}

u64 Platform_SchedulerGetFrameCounter(void)
{
    return sFrameCounter;
}

void Platform_SchedulerSetFrameCounter(u64 frame)
{
    sFrameCounter = frame;
}

void Platform_SchedulerShutdown(void)
{
    /* AgbMain has no cooperative shutdown point; the process is exiting here. */
}

#endif
