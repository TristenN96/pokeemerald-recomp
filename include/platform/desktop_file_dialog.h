#ifndef GUARD_PLATFORM_DESKTOP_FILE_DIALOG_H
#define GUARD_PLATFORM_DESKTOP_FILE_DIALOG_H

#include "gba/types.h"

enum PlatformFileDialogResult
{
    PLATFORM_FILE_DIALOG_CANCELLED,
    PLATFORM_FILE_DIALOG_SELECTED,
    PLATFORM_FILE_DIALOG_NO_PICKER_AVAILABLE,
    PLATFORM_FILE_DIALOG_FAILED,
};

enum PlatformFileDialogResult Platform_FileDialogSelectEmeraldRom(char *path, u32 pathSize);

#endif
