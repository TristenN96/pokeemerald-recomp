#ifndef GUARD_PLATFORM_DESKTOP_STATE_H
#define GUARD_PLATFORM_DESKTOP_STATE_H

#include "gba/types.h"
#include "platform/desktop_state_metadata.h"

enum PlatformStateOperationResult
{
    PLATFORM_STATE_OPERATION_FAILED,
    PLATFORM_STATE_OPERATION_OK,
    PLATFORM_STATE_OPERATION_OK_WITH_WARNINGS
};

struct PlatformStateEntry
{
    u8 slot;
    bool32 occupied;
    bool32 thumbnailAvailable;
    bool32 metadataAvailable;
    bool32 profileConflict;
    struct PlatformStateMetadata metadata;
};

void Platform_StateGetEntry(u8 slot, struct PlatformStateEntry *entry);
enum PlatformStateOperationResult Platform_StateSave(u8 slot);
enum PlatformStateOperationResult Platform_StateLoad(u8 slot);
enum PlatformStateOperationResult Platform_StateDelete(u8 slot);
const char *Platform_StateGetLastError(void);

#endif
