#ifndef GUARD_PLATFORM_DESKTOP_PROFILES_H
#define GUARD_PLATFORM_DESKTOP_PROFILES_H

#include "gba/types.h"

#define PLATFORM_PROFILE_ID_LENGTH 32
#define PLATFORM_PROFILE_NAME_LENGTH 64
#define PLATFORM_PROFILE_DATE_LENGTH 32
#define PLATFORM_PROFILE_RULESET_LENGTH 64
#define PLATFORM_PROFILE_SEED_LENGTH 64
#define PLATFORM_PROFILE_DIFFICULTY_LENGTH 32
#define PLATFORM_PROFILE_MODS_LENGTH 256
#define PLATFORM_PROFILE_FINGERPRINT_LENGTH 128
#define PLATFORM_STATE_SLOT_COUNT 8
#define PLATFORM_STATE_QUICK_SLOT 0

enum PlatformStateFileKind
{
    PLATFORM_STATE_FILE_NATIVE,
    PLATFORM_STATE_FILE_THUMBNAIL,
    PLATFORM_STATE_FILE_METADATA,
    PLATFORM_STATE_FILE_COUNT
};

struct PlatformProfileMetadata
{
    u32 schemaVersion;
    char id[PLATFORM_PROFILE_ID_LENGTH];
    char displayName[PLATFORM_PROFILE_NAME_LENGTH];
    char createdTime[PLATFORM_PROFILE_DATE_LENGTH];
    char lastPlayedTime[PLATFORM_PROFILE_DATE_LENGTH];
    char gameMode[PLATFORM_PROFILE_NAME_LENGTH];
    char ruleset[PLATFORM_PROFILE_RULESET_LENGTH];
    char randomizerSeed[PLATFORM_PROFILE_SEED_LENGTH];
    char difficultyPreset[PLATFORM_PROFILE_DIFFICULTY_LENGTH];
    char enabledMods[PLATFORM_PROFILE_MODS_LENGTH];
    char contentFingerprint[PLATFORM_PROFILE_FINGERPRINT_LENGTH];
};

bool32 Platform_ProfileInit(void);
u32 Platform_ProfileCount(void);
const struct PlatformProfileMetadata *Platform_ProfileGet(u32 index);
u32 Platform_ProfileGetSelectedIndex(void);
const struct PlatformProfileMetadata *Platform_ProfileGetSelected(void);
bool32 Platform_ProfileSelect(u32 index);
bool32 Platform_ProfileCreate(const char *displayName, u32 *createdIndex);
bool32 Platform_ProfileRename(u32 index, const char *displayName);
bool32 Platform_ProfileDelete(u32 index);
bool32 Platform_ProfileLoadSelectedSave(u8 *image, u32 size);
bool32 Platform_ProfileWriteSelectedSave(const u8 *image, u32 size);
bool32 Platform_ProfileGetStateSlotId(u8 slot, char *slotId, u32 slotIdSize);
bool32 Platform_ProfileGetStateFilePath(u8 slot, enum PlatformStateFileKind kind,
                                        char *path, u32 pathSize);
bool32 Platform_ProfileGetStatePath(u8 slot, char *path, u32 pathSize);

#endif
