#ifdef PLATFORM_SDL2

#include <string.h>
#include <time.h>

#include "global.h"
#include "platform/desktop_clock.h"

HOST_DATA static struct SiiRtcInfo sInternalClock;
HOST_DATA static PlatformClockProvider sClockProvider;

static u8 BinToBcd(u8 bin)
{
    int placeCounter = 1;
    u8 out = 0;
    do
    {
        out |= (bin % 10) * placeCounter;
        placeCounter *= 16;
    }
    while ((bin /= 10) > 0);
    return out;
}

static void UpdateInternalClock(void)
{
    time_t rawTime = time(NULL);
    struct tm *localTime = localtime(&rawTime);

    if (localTime == NULL)
        return;
    sInternalClock.year = BinToBcd(localTime->tm_year - 100);
    sInternalClock.month = BinToBcd(localTime->tm_mon + 1);
    sInternalClock.day = BinToBcd(localTime->tm_mday);
    sInternalClock.dayOfWeek = BinToBcd(localTime->tm_wday);
    sInternalClock.hour = BinToBcd(localTime->tm_hour);
    sInternalClock.minute = BinToBcd(localTime->tm_min);
    sInternalClock.second = BinToBcd(localTime->tm_sec);
}

void Platform_ClockInit(void)
{
    memset(&sInternalClock, 0, sizeof(sInternalClock));
    sInternalClock.status = SIIRTCINFO_24HOUR;
    sClockProvider = NULL;
    UpdateInternalClock();
}

void Platform_ClockSetProvider(PlatformClockProvider provider)
{
    sClockProvider = provider;
}

void Platform_ClockResetProvider(void)
{
    sClockProvider = NULL;
}

void Platform_GetStatus(struct SiiRtcInfo *rtc)
{
    rtc->status = sInternalClock.status;
}

void Platform_SetStatus(struct SiiRtcInfo *rtc)
{
    sInternalClock.status = rtc->status;
}

void Platform_GetDateTime(struct SiiRtcInfo *rtc)
{
    if (sClockProvider != NULL)
    {
        struct SiiRtcInfo provided = {0};
        provided.status = sInternalClock.status;
        if (sClockProvider(&provided))
        {
            provided.status = sInternalClock.status;
            *rtc = provided;
            return;
        }
    }
    UpdateInternalClock();
    rtc->year = sInternalClock.year;
    rtc->month = sInternalClock.month;
    rtc->day = sInternalClock.day;
    rtc->dayOfWeek = sInternalClock.dayOfWeek;
    rtc->hour = sInternalClock.hour;
    rtc->minute = sInternalClock.minute;
    rtc->second = sInternalClock.second;
    rtc->status = sInternalClock.status;
}

void Platform_SetDateTime(struct SiiRtcInfo *rtc)
{
    sInternalClock.month = rtc->month;
    sInternalClock.day = rtc->day;
    sInternalClock.dayOfWeek = rtc->dayOfWeek;
    sInternalClock.hour = rtc->hour;
    sInternalClock.minute = rtc->minute;
    sInternalClock.second = rtc->second;
}

void Platform_GetTime(struct SiiRtcInfo *rtc)
{
    struct SiiRtcInfo dateTime;
    Platform_GetDateTime(&dateTime);
    rtc->hour = dateTime.hour;
    rtc->minute = dateTime.minute;
    rtc->second = dateTime.second;
}

void Platform_SetTime(struct SiiRtcInfo *rtc)
{
    sInternalClock.hour = rtc->hour;
    sInternalClock.minute = rtc->minute;
    sInternalClock.second = rtc->second;
}

void Platform_SetAlarm(u8 *alarmData)
{
    (void)alarmData;
}

bool32 Platform_ClockCopyState(void *dest, u32 size)
{
    if (dest == NULL || size != sizeof(sInternalClock))
        return FALSE;
    memcpy(dest, &sInternalClock, sizeof(sInternalClock));
    return TRUE;
}

bool32 Platform_ClockRestoreState(const void *source, u32 size)
{
    if (source == NULL || size != sizeof(sInternalClock))
        return FALSE;
    memcpy(&sInternalClock, source, sizeof(sInternalClock));
    return TRUE;
}

#endif
