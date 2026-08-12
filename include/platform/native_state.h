#ifndef GUARD_PLATFORM_NATIVE_STATE_H
#define GUARD_PLATFORM_NATIVE_STATE_H

#include "gba/types.h"
#include "platform/desktop_profiles.h"

enum NativeStateResult
{
    NATIVE_STATE_OK,
    NATIVE_STATE_UNAVAILABLE,
    NATIVE_STATE_IO_ERROR,
    NATIVE_STATE_INCOMPATIBLE,
    NATIVE_STATE_CORRUPT,
    NATIVE_STATE_UNSUPPORTED,
};

enum NativeStateResult NativeState_Save(u8 slot);
enum NativeStateResult NativeState_Load(u8 slot);
u32 NativeState_GetFormatVersion(void);
const char *NativeState_GetLastError(void);

#endif
