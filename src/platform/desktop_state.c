#ifdef PLATFORM_SDL2

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_state.h"
#include "platform/desktop_state_metadata.h"
#include "platform/desktop_state_thumbnail.h"
#include "platform/desktop_storage.h"
#include "platform/native_state.h"

HOST_DATA static char sStateError[256];

static void SetError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(sStateError, sizeof(sStateError), format, args);
    va_end(args);
}

static bool32 GetPath(u8 slot, enum PlatformStateFileKind kind, char *path, u32 pathSize)
{
    if (slot > PLATFORM_STATE_SLOT_COUNT
     || !Platform_ProfileGetStateFilePath(slot, kind, path, pathSize))
    {
        SetError("No active profile is available for this state slot");
        return FALSE;
    }
    return TRUE;
}

void Platform_StateGetEntry(u8 slot, struct PlatformStateEntry *entry)
{
    const struct PlatformProfileMetadata *profile = Platform_ProfileGetSelected();
    char path[1024];

    memset(entry, 0, sizeof(*entry));
    entry->slot = slot;
    if (!GetPath(slot, PLATFORM_STATE_FILE_NATIVE, path, sizeof(path)))
        return;
    entry->occupied = Platform_StorageFileExists(path);
    if (!entry->occupied)
        return;
    if (GetPath(slot, PLATFORM_STATE_FILE_THUMBNAIL, path, sizeof(path)))
        entry->thumbnailAvailable = Platform_StorageFileExists(path);
    entry->metadataAvailable = Platform_StateMetadataRead(slot, &entry->metadata);
    if (entry->metadataAvailable && profile != NULL
     && strcmp(entry->metadata.profileId, profile->id) != 0)
        entry->profileConflict = TRUE;
}

enum PlatformStateOperationResult Platform_StateSave(u8 slot)
{
    struct PlatformStateMetadata metadata;
    enum NativeStateResult nativeResult;
    char path[1024];
    bool32 thumbnailWritten;
    bool32 metadataWritten;

    sStateError[0] = '\0';
    if (!GetPath(slot, PLATFORM_STATE_FILE_NATIVE, path, sizeof(path)))
        return PLATFORM_STATE_OPERATION_FAILED;
    nativeResult = NativeState_Save(slot);
    if (nativeResult != NATIVE_STATE_OK)
    {
        SetError("%s", NativeState_GetLastError());
        return PLATFORM_STATE_OPERATION_FAILED;
    }

    Platform_StateMetadataCapture(slot, &metadata);
    thumbnailWritten = Platform_StateThumbnailCapture(slot);
    metadataWritten = Platform_StateMetadataWrite(slot, &metadata);
    if (!thumbnailWritten
     && GetPath(slot, PLATFORM_STATE_FILE_THUMBNAIL, path, sizeof(path)))
        Platform_StorageRemoveFile(path);
    if (!metadataWritten
     && GetPath(slot, PLATFORM_STATE_FILE_METADATA, path, sizeof(path)))
        Platform_StorageRemoveFile(path);
    if (!thumbnailWritten || !metadataWritten)
    {
        SetError("State saved, but %s%s could not be written",
                 !thumbnailWritten ? "thumbnail" : "",
                 !thumbnailWritten && !metadataWritten ? " and metadata" :
                 (!metadataWritten ? "metadata" : ""));
        return PLATFORM_STATE_OPERATION_OK_WITH_WARNINGS;
    }
    return PLATFORM_STATE_OPERATION_OK;
}

enum PlatformStateOperationResult Platform_StateLoad(u8 slot)
{
    const struct PlatformProfileMetadata *profile = Platform_ProfileGetSelected();
    struct PlatformStateMetadata metadata;
    enum NativeStateResult nativeResult;
    char path[1024];

    sStateError[0] = '\0';
    if (!GetPath(slot, PLATFORM_STATE_FILE_NATIVE, path, sizeof(path)))
        return PLATFORM_STATE_OPERATION_FAILED;
    if (!Platform_StorageFileExists(path))
    {
        SetError("Selected state is empty");
        return PLATFORM_STATE_OPERATION_FAILED;
    }
    /* PNG and JSON are companions, not load dependencies. A readable metadata
     * file is used only as a profile consistency guard. */
    if (Platform_StateMetadataRead(slot, &metadata) && profile != NULL
     && strcmp(metadata.profileId, profile->id) != 0)
    {
        SetError("State belongs to profile '%s', not active profile '%s'",
                 metadata.profileId, profile->id);
        return PLATFORM_STATE_OPERATION_FAILED;
    }
    nativeResult = NativeState_Load(slot);
    if (nativeResult != NATIVE_STATE_OK)
    {
        SetError("%s", NativeState_GetLastError());
        return PLATFORM_STATE_OPERATION_FAILED;
    }
    return PLATFORM_STATE_OPERATION_OK;
}

enum PlatformStateOperationResult Platform_StateDelete(u8 slot)
{
    enum PlatformStateFileKind kind;
    bool32 success = TRUE;
    char path[1024];

    sStateError[0] = '\0';
    for (kind = PLATFORM_STATE_FILE_NATIVE; kind < PLATFORM_STATE_FILE_COUNT; kind++)
    {
        if (!GetPath(slot, kind, path, sizeof(path))
         || !Platform_StorageRemoveFile(path))
            success = FALSE;
    }
    if (!success)
    {
        SetError("One or more state files could not be deleted");
        return PLATFORM_STATE_OPERATION_FAILED;
    }
    return PLATFORM_STATE_OPERATION_OK;
}

const char *Platform_StateGetLastError(void)
{
    return sStateError;
}

#endif
