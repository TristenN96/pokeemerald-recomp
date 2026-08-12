#ifndef GUARD_PLATFORM_DESKTOP_STATE_METADATA_H
#define GUARD_PLATFORM_DESKTOP_STATE_METADATA_H

#include "gba/types.h"
#include "platform/desktop_profiles.h"

#define PLATFORM_STATE_TIMESTAMP_LENGTH 40
#define PLATFORM_STATE_LOCATION_LENGTH 64
#define PLATFORM_STATE_CONTEXT_LENGTH 16
#define PLATFORM_STATE_SLOT_ID_LENGTH 16

struct PlatformStateMetadata
{
    u32 metadataVersion;
    char slotId[PLATFORM_STATE_SLOT_ID_LENGTH];
    char profileId[PLATFORM_PROFILE_ID_LENGTH];
    char timestamp[PLATFORM_STATE_TIMESTAMP_LENGTH];
    u64 playTimeSeconds;
    char location[PLATFORM_STATE_LOCATION_LENGTH];
    char context[PLATFORM_STATE_CONTEXT_LENGTH];
    u32 stateVersion;
};

void Platform_StateMetadataCapture(u8 slot, struct PlatformStateMetadata *metadata);
bool32 Platform_StateMetadataWrite(u8 slot, const struct PlatformStateMetadata *metadata);
bool32 Platform_StateMetadataRead(u8 slot, struct PlatformStateMetadata *metadata);
void Platform_StateMetadataFormatPlayTime(const struct PlatformStateMetadata *metadata,
                                          char *dest, u32 destSize);
void Platform_StateMetadataFormatTimestamp(const struct PlatformStateMetadata *metadata,
                                           char *dest, u32 destSize);

#endif
