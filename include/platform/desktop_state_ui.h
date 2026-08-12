#ifndef GUARD_PLATFORM_DESKTOP_STATE_UI_H
#define GUARD_PLATFORM_DESKTOP_STATE_UI_H

enum PlatformStateUiResult
{
    PLATFORM_STATE_UI_CLOSED,
    PLATFORM_STATE_UI_SAVED,
    PLATFORM_STATE_UI_LOADED,
    PLATFORM_STATE_UI_QUIT
};

enum PlatformStateUiResult Platform_StateUiRunManager(void);
enum PlatformStateUiResult Platform_StateUiRunSavePicker(void);

#endif
