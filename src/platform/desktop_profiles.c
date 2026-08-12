#ifdef PLATFORM_SDL2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "global.h"
#include "gba/flash_internal.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_filesystem.h"
#include "platform/desktop_storage.h"

#define PROFILE_LIMIT 64
#define PROFILE_PATH_LENGTH 1024

HOST_DATA static struct PlatformProfileMetadata sProfiles[PROFILE_LIMIT];
HOST_DATA static u32 sProfileCount;
HOST_DATA static u32 sSelectedIndex;
HOST_DATA static char sProfilesRoot[PROFILE_PATH_LENGTH];

static void BuildPath(char *dest, u32 destSize, const char *id, const char *fileName)
{
    snprintf(dest, destSize, "%s/profiles/%s/%s", Platform_StorageGetRootPath(), id, fileName);
}

static void BuildDirectory(char *dest, u32 destSize, const char *id)
{
    snprintf(dest, destSize, "%s/profiles/%s", Platform_StorageGetRootPath(), id);
}

static void BuildStatesDirectory(char *dest, u32 destSize, const char *id)
{
    snprintf(dest, destSize, "%s/profiles/%s/states", Platform_StorageGetRootPath(), id);
}

static bool32 BuildStateSlotId(char *dest, u32 destSize, u8 slot)
{
    int length;
    if (slot > PLATFORM_STATE_SLOT_COUNT)
        return FALSE;
    length = slot == PLATFORM_STATE_QUICK_SLOT
        ? snprintf(dest, destSize, "quick")
        : snprintf(dest, destSize, "slot%u", slot);
    return length > 0 && (u32)length < destSize;
}

static bool32 BuildStateFilePath(char *dest, u32 destSize, const char *id, u8 slot,
                                 enum PlatformStateFileKind kind)
{
    static const char *const extensions[PLATFORM_STATE_FILE_COUNT] =
    {
        "state", "png", "json"
    };
    char slotId[16];
    int length;

    if (kind >= PLATFORM_STATE_FILE_COUNT
     || !BuildStateSlotId(slotId, sizeof(slotId), slot))
        return FALSE;
    length = snprintf(dest, destSize, "%s/profiles/%s/states/%s.%s",
                      Platform_StorageGetRootPath(), id, slotId, extensions[kind]);
    return length > 0 && (u32)length < destSize;
}

static void BuildSelectedMarkerPath(char *dest, u32 destSize)
{
    snprintf(dest, destSize, "%s/selected_profile", Platform_StorageGetRootPath());
}

static void CopyText(char *dest, u32 destSize, const char *source)
{
    u32 i;
    if (destSize == 0)
        return;
    for (i = 0; i + 1 < destSize && source != NULL && source[i] != '\0'; i++)
    {
        char c = source[i];
        dest[i] = (c == '=' || c == '\n' || c == '\r') ? ' ' : c;
    }
    dest[i] = '\0';
}

static void CurrentTime(char *dest, u32 destSize)
{
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    if (utc == NULL || strftime(dest, destSize, "%Y-%m-%dT%H:%M:%SZ", utc) == 0)
        CopyText(dest, destSize, "unknown");
}

static void InitMetadata(struct PlatformProfileMetadata *metadata, const char *id, const char *name)
{
    memset(metadata, 0, sizeof(*metadata));
    metadata->schemaVersion = 1;
    CopyText(metadata->id, sizeof(metadata->id), id);
    CopyText(metadata->displayName, sizeof(metadata->displayName), name);
    CurrentTime(metadata->createdTime, sizeof(metadata->createdTime));
    CopyText(metadata->lastPlayedTime, sizeof(metadata->lastPlayedTime), metadata->createdTime);
}

static bool32 ParseLine(char *line, char **key, char **value)
{
    char *equals = strchr(line, '=');
    char *end;
    if (equals == NULL)
        return FALSE;
    *equals = '\0';
    *key = line;
    *value = equals + 1;
    end = *value + strlen(*value);
    while (end > *value && (end[-1] == '\n' || end[-1] == '\r'))
        *--end = '\0';
    return TRUE;
}

static void AssignMetadataField(struct PlatformProfileMetadata *metadata, const char *key, const char *value)
{
    if (strcmp(key, "schemaVersion") == 0)
        metadata->schemaVersion = (u32)strtoul(value, NULL, 10);
    else if (strcmp(key, "id") == 0)
        CopyText(metadata->id, sizeof(metadata->id), value);
    else if (strcmp(key, "displayName") == 0)
        CopyText(metadata->displayName, sizeof(metadata->displayName), value);
    else if (strcmp(key, "createdTime") == 0)
        CopyText(metadata->createdTime, sizeof(metadata->createdTime), value);
    else if (strcmp(key, "lastPlayedTime") == 0)
        CopyText(metadata->lastPlayedTime, sizeof(metadata->lastPlayedTime), value);
    else if (strcmp(key, "gameMode") == 0)
        CopyText(metadata->gameMode, sizeof(metadata->gameMode), value);
    else if (strcmp(key, "ruleset") == 0)
        CopyText(metadata->ruleset, sizeof(metadata->ruleset), value);
    else if (strcmp(key, "randomizerSeed") == 0)
        CopyText(metadata->randomizerSeed, sizeof(metadata->randomizerSeed), value);
    else if (strcmp(key, "difficultyPreset") == 0)
        CopyText(metadata->difficultyPreset, sizeof(metadata->difficultyPreset), value);
    else if (strcmp(key, "enabledMods") == 0)
        CopyText(metadata->enabledMods, sizeof(metadata->enabledMods), value);
    else if (strcmp(key, "contentFingerprint") == 0)
        CopyText(metadata->contentFingerprint, sizeof(metadata->contentFingerprint), value);
}

static bool32 LoadMetadata(const char *id, struct PlatformProfileMetadata *metadata)
{
    char path[PROFILE_PATH_LENGTH];
    FILE *file;
    char line[512];
    bool32 hasName = FALSE;

    BuildPath(path, sizeof(path), id, "metadata.ini");
    file = Platform_FileOpen(path, "r");
    if (file == NULL)
        return FALSE;
    memset(metadata, 0, sizeof(*metadata));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *key;
        char *value;
        if (ParseLine(line, &key, &value))
        {
            AssignMetadataField(metadata, key, value);
            if (strcmp(key, "displayName") == 0)
                hasName = TRUE;
        }
    }
    fclose(file);
    if (metadata->schemaVersion == 0 || metadata->schemaVersion > 1 || !hasName)
        return FALSE;
    if (metadata->id[0] == '\0')
        CopyText(metadata->id, sizeof(metadata->id), id);
    if (metadata->displayName[0] == '\0')
        CopyText(metadata->displayName, sizeof(metadata->displayName), metadata->id);
    return TRUE;
}

static bool32 StoreMetadata(const struct PlatformProfileMetadata *metadata)
{
    char path[PROFILE_PATH_LENGTH];
    char text[1536];
    int length;

    BuildPath(path, sizeof(path), metadata->id, "metadata.ini");
    length = snprintf(text, sizeof(text),
                      "schemaVersion=%u\n"
                      "id=%s\n"
                      "displayName=%s\n"
                      "createdTime=%s\n"
                      "lastPlayedTime=%s\n"
                      "gameMode=%s\n"
                      "ruleset=%s\n"
                      "randomizerSeed=%s\n"
                      "difficultyPreset=%s\n"
                      "enabledMods=%s\n"
                      "contentFingerprint=%s\n",
                      metadata->schemaVersion, metadata->id, metadata->displayName,
                      metadata->createdTime, metadata->lastPlayedTime, metadata->gameMode,
                      metadata->ruleset, metadata->randomizerSeed, metadata->difficultyPreset,
                      metadata->enabledMods, metadata->contentFingerprint);
    return length > 0 && (size_t)length < sizeof(text)
        && Platform_StorageWriteAtomic(path, text, (u32)length);
}

static bool32 CreateWithId(const char *id, const char *name, const char *legacyPath,
                           u32 *createdIndex)
{
    char directory[PROFILE_PATH_LENGTH];
    char savePath[PROFILE_PATH_LENGTH];
    char statesPath[PROFILE_PATH_LENGTH];
    struct PlatformProfileMetadata metadata;
    HOST_DATA static u8 blankSave[131072];

    if (sProfileCount >= PROFILE_LIMIT)
        return FALSE;
    BuildDirectory(directory, sizeof(directory), id);
    if (!Platform_StorageEnsureDirectory(directory))
        return FALSE;
    BuildStatesDirectory(statesPath, sizeof(statesPath), id);
    if (!Platform_StorageEnsureDirectory(statesPath))
        return FALSE;
    InitMetadata(&metadata, id, name);
    BuildPath(savePath, sizeof(savePath), id, "game.sav");
    if (legacyPath != NULL && Platform_StorageFileExists(legacyPath))
    {
        /* Normalize the imported image to the exact vanilla flash size. This
         * preserves every byte the game can use while ensuring every profile
         * owns a real 128 KiB game.sav, even for an empty legacy file. */
        if (!Platform_StorageLoadSaveFromPath(legacyPath, blankSave, sizeof(blankSave))
         || !Platform_StorageWriteSaveToPath(savePath, blankSave, sizeof(blankSave)))
            return FALSE;
    }
    else
    {
        memset(blankSave, 0xFF, sizeof(blankSave));
        if (!Platform_StorageWriteSaveToPath(savePath, blankSave, sizeof(blankSave)))
            return FALSE;
    }
    if (!StoreMetadata(&metadata))
        return FALSE;
    sProfiles[sProfileCount] = metadata;
    if (createdIndex != NULL)
        *createdIndex = sProfileCount;
    sProfileCount++;
    return TRUE;
}

static void ScanProfiles(void)
{
    u32 i;
    char id[PLATFORM_PROFILE_ID_LENGTH];
    sProfileCount = 0;
    if (LoadMetadata("default", &sProfiles[sProfileCount]))
        sProfileCount++;
    for (i = 1; i <= 9999 && sProfileCount < PROFILE_LIMIT; i++)
    {
        snprintf(id, sizeof(id), "profile-%04u", i);
        if (LoadMetadata(id, &sProfiles[sProfileCount]))
            sProfileCount++;
    }
}

bool32 Platform_ProfileInit(void)
{
    const char *legacyPath = NULL;
    bool32 haveLegacy;
    char markerPath[PROFILE_PATH_LENGTH];
    char marker[PLATFORM_PROFILE_ID_LENGTH];
    FILE *markerFile;

    snprintf(sProfilesRoot, sizeof(sProfilesRoot), "%s/profiles", Platform_StorageGetRootPath());
    if (!Platform_StorageEnsureDirectory(sProfilesRoot))
        return FALSE;
    ScanProfiles();
    if (sProfileCount == 0)
    {
        if (Platform_StorageFileExists(Platform_StorageGetPreferredLegacySavePath()))
            legacyPath = Platform_StorageGetPreferredLegacySavePath();
        else if (Platform_StorageFileExists(Platform_StorageGetLegacySavePath()))
            legacyPath = Platform_StorageGetLegacySavePath();
        haveLegacy = legacyPath != NULL;
        if (!CreateWithId("default", "Default", haveLegacy ? legacyPath : NULL, NULL))
            return FALSE;
        if (haveLegacy)
            DBGPRINTF("Imported legacy save into profile default; original retained at %s\n", legacyPath);
    }
    sSelectedIndex = 0;
    BuildSelectedMarkerPath(markerPath, sizeof(markerPath));
    markerFile = Platform_FileOpen(markerPath, "r");
    if (markerFile != NULL)
    {
        if (fgets(marker, sizeof(marker), markerFile) != NULL)
        {
            u32 i;
            marker[strcspn(marker, "\r\n")] = '\0';
            for (i = 0; i < sProfileCount; i++)
            {
                if (strcmp(marker, sProfiles[i].id) == 0)
                {
                    sSelectedIndex = i;
                    break;
                }
            }
        }
        fclose(markerFile);
    }
    return Platform_ProfileSelect(sSelectedIndex);
}

u32 Platform_ProfileCount(void)
{
    return sProfileCount;
}

const struct PlatformProfileMetadata *Platform_ProfileGet(u32 index)
{
    return index < sProfileCount ? &sProfiles[index] : NULL;
}

u32 Platform_ProfileGetSelectedIndex(void)
{
    return sSelectedIndex;
}

const struct PlatformProfileMetadata *Platform_ProfileGetSelected(void)
{
    return Platform_ProfileGet(sSelectedIndex);
}

bool32 Platform_ProfileSelect(u32 index)
{
    char savePath[PROFILE_PATH_LENGTH];
    char statesPath[PROFILE_PATH_LENGTH];
    char markerPath[PROFILE_PATH_LENGTH];
    if (index >= sProfileCount)
        return FALSE;
    sSelectedIndex = index;
    BuildPath(savePath, sizeof(savePath), sProfiles[index].id, "game.sav");
    if (!Platform_StorageSetSavePath(savePath))
        return FALSE;
    BuildStatesDirectory(statesPath, sizeof(statesPath), sProfiles[index].id);
    if (!Platform_StorageEnsureDirectory(statesPath))
        return FALSE;
    CurrentTime(sProfiles[index].lastPlayedTime, sizeof(sProfiles[index].lastPlayedTime));
    StoreMetadata(&sProfiles[index]);
    BuildSelectedMarkerPath(markerPath, sizeof(markerPath));
    Platform_StorageWriteAtomic(markerPath, sProfiles[index].id, strlen(sProfiles[index].id));
    return TRUE;
}

bool32 Platform_ProfileCreate(const char *displayName, u32 *createdIndex)
{
    char id[PLATFORM_PROFILE_ID_LENGTH];
    u32 i;
    if (displayName == NULL || displayName[0] == '\0')
        return FALSE;
    for (i = 1; i <= 9999; i++)
    {
        char path[PROFILE_PATH_LENGTH];
        snprintf(id, sizeof(id), "profile-%04u", i);
        BuildPath(path, sizeof(path), id, "metadata.ini");
        if (!Platform_StorageFileExists(path))
            return CreateWithId(id, displayName, NULL, createdIndex);
    }
    return FALSE;
}

bool32 Platform_ProfileRename(u32 index, const char *displayName)
{
    if (index >= sProfileCount || displayName == NULL || displayName[0] == '\0')
        return FALSE;
    CopyText(sProfiles[index].displayName, sizeof(sProfiles[index].displayName), displayName);
    return StoreMetadata(&sProfiles[index]);
}

bool32 Platform_ProfileDelete(u32 index)
{
    char path[PROFILE_PATH_LENGTH];
    if (index >= sProfileCount || sProfileCount <= 1)
        return FALSE;
    {
        u8 slot;
        for (slot = PLATFORM_STATE_QUICK_SLOT; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
        {
            enum PlatformStateFileKind kind;
            for (kind = PLATFORM_STATE_FILE_NATIVE; kind < PLATFORM_STATE_FILE_COUNT; kind++)
            {
                if (!BuildStateFilePath(path, sizeof(path), sProfiles[index].id, slot, kind)
                 || !Platform_StorageRemoveFile(path))
                    return FALSE;
            }
        }
    }
    BuildPath(path, sizeof(path), sProfiles[index].id, "game.sav");
    if (!Platform_StorageRemoveFile(path))
        return FALSE;
    BuildPath(path, sizeof(path), sProfiles[index].id, "metadata.ini");
    if (!Platform_StorageRemoveFile(path))
        return FALSE;
    BuildStatesDirectory(path, sizeof(path), sProfiles[index].id);
    if (!Platform_StorageRemoveDirectory(path))
        return FALSE;
    BuildDirectory(path, sizeof(path), sProfiles[index].id);
    if (!Platform_StorageRemoveDirectory(path))
        return FALSE;
    memmove(&sProfiles[index], &sProfiles[index + 1],
            (sProfileCount - index - 1) * sizeof(sProfiles[0]));
    sProfileCount--;
    if (sSelectedIndex >= sProfileCount)
        sSelectedIndex = sProfileCount - 1;
    return Platform_ProfileSelect(sSelectedIndex);
}

bool32 Platform_ProfileLoadSelectedSave(u8 *image, u32 size)
{
    return Platform_StorageLoadSave(image, size);
}

bool32 Platform_ProfileWriteSelectedSave(const u8 *image, u32 size)
{
    return Platform_StorageWriteSave(image, size);
}

bool32 Platform_ProfileGetStateSlotId(u8 slot, char *slotId, u32 slotIdSize)
{
    return slotId != NULL && BuildStateSlotId(slotId, slotIdSize, slot);
}

bool32 Platform_ProfileGetStateFilePath(u8 slot, enum PlatformStateFileKind kind,
                                        char *path, u32 pathSize)
{
    const struct PlatformProfileMetadata *profile = Platform_ProfileGetSelected();
    return profile != NULL && path != NULL
        && BuildStateFilePath(path, pathSize, profile->id, slot, kind);
}

bool32 Platform_ProfileGetStatePath(u8 slot, char *path, u32 pathSize)
{
    return Platform_ProfileGetStateFilePath(slot, PLATFORM_STATE_FILE_NATIVE, path, pathSize);
}

#endif
