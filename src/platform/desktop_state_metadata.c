#ifdef PLATFORM_SDL2

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "global.h"
#include "main.h"
#include "overworld.h"
#include "region_map.h"
#include "script.h"
#include "text.h"
#include "constants/characters.h"
#include "constants/region_map_sections.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_state_metadata.h"
#include "platform/desktop_storage.h"
#include "platform/native_state.h"

#define STATE_METADATA_FILE_CAPACITY 4096

static void CopyText(char *dest, u32 destSize, const char *source)
{
    if (dest == NULL || destSize == 0)
        return;
    if (source == NULL)
        source = "";
    snprintf(dest, destSize, "%s", source);
}

static void FormatLocalTimestamp(char *dest, u32 destSize)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char date[32];
    char zone[16];

    if (local == NULL
     || strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", local) == 0
     || strftime(zone, sizeof(zone), "%z", local) == 0)
    {
        CopyText(dest, destSize, "unknown");
        return;
    }
    if (strlen(zone) == 5 && (zone[0] == '+' || zone[0] == '-'))
        snprintf(dest, destSize, "%s%c%c%c:%c%c", date, zone[0], zone[1], zone[2], zone[3], zone[4]);
    else
        snprintf(dest, destSize, "%s%s", date, zone);
}

static void GameTextToAscii(const u8 *source, char *dest, u32 destSize)
{
    u32 output = 0;
    u32 input = 0;

    if (destSize == 0)
        return;
    while (source != NULL && source[input] != EOS && output + 1 < destSize)
    {
        u8 c = source[input++];
        char converted = '?';
        if (c == CHAR_SPACE || c == CHAR_SPACER)
            converted = ' ';
        else if (c >= CHAR_0 && c <= CHAR_9)
            converted = '0' + c - CHAR_0;
        else if (c >= CHAR_A && c <= CHAR_Z)
            converted = 'A' + c - CHAR_A;
        else if (c >= CHAR_a && c <= CHAR_z)
            converted = 'a' + c - CHAR_a;
        else if (c == CHAR_HYPHEN)
            converted = '-';
        else if (c == CHAR_PERIOD)
            converted = '.';
        else if (c == CHAR_COMMA)
            converted = ',';
        else if (c == CHAR_SLASH)
            converted = '/';
        else if (c == CHAR_COLON)
            converted = ':';
        else if (c == CHAR_SGL_QUOTE_LEFT || c == CHAR_SGL_QUOTE_RIGHT)
            converted = '\'';
        dest[output++] = converted;
    }
    while (output > 0 && dest[output - 1] == ' ')
        output--;
    dest[output] = '\0';
}

static void CaptureLocation(char *dest, u32 destSize)
{
    u8 mapName[32];

    CopyText(dest, destSize, "Unknown");
    if (gSaveBlock1Ptr == NULL || gMapHeader.regionMapSectionId >= MAPSEC_NONE)
        return;
    GetMapNameGeneric(mapName, gMapHeader.regionMapSectionId);
    GameTextToAscii(mapName, dest, destSize);
    if (dest[0] == '\0')
        CopyText(dest, destSize, "Unknown");
}

static const char *CaptureContext(void)
{
    if (gMain.inBattle)
        return "battle";
    if (IsTextPrinterActive(0))
        return "dialogue";
    if (gMain.callback2 == CB2_Overworld || gMain.callback2 == CB2_OverworldBasic)
        return ScriptContext_IsEnabled() ? "dialogue" : "overworld";
    if (gMain.callback1 != NULL || gMain.callback2 != NULL)
        return "menu";
    return "other";
}

void Platform_StateMetadataCapture(u8 slot, struct PlatformStateMetadata *metadata)
{
    const struct PlatformProfileMetadata *profile = Platform_ProfileGetSelected();

    memset(metadata, 0, sizeof(*metadata));
    metadata->metadataVersion = 1;
    Platform_ProfileGetStateSlotId(slot, metadata->slotId, sizeof(metadata->slotId));
    CopyText(metadata->profileId, sizeof(metadata->profileId), profile != NULL ? profile->id : "unknown");
    FormatLocalTimestamp(metadata->timestamp, sizeof(metadata->timestamp));
    if (gSaveBlock2Ptr != NULL)
    {
        metadata->playTimeSeconds = (u64)gSaveBlock2Ptr->playTimeHours * 3600
                                  + (u64)gSaveBlock2Ptr->playTimeMinutes * 60
                                  + gSaveBlock2Ptr->playTimeSeconds;
    }
    CaptureLocation(metadata->location, sizeof(metadata->location));
    CopyText(metadata->context, sizeof(metadata->context), CaptureContext());
    metadata->stateVersion = NativeState_GetFormatVersion();
}

static bool32 AppendJsonString(char *dest, u32 destSize, u32 *offset, const char *source)
{
    u32 i;
    for (i = 0; source != NULL && source[i] != '\0'; i++)
    {
        const char *escape = NULL;
        char encoded[2] = {source[i], '\0'};
        int written;
        if (source[i] == '"') escape = "\\\"";
        else if (source[i] == '\\') escape = "\\\\";
        else if (source[i] == '\n') escape = "\\n";
        else if (source[i] == '\r') escape = "\\r";
        else if (source[i] == '\t') escape = "\\t";
        else if ((unsigned char)source[i] < 32) encoded[0] = ' ';
        written = snprintf(dest + *offset, destSize - *offset, "%s", escape != NULL ? escape : encoded);
        if (written < 0 || (u32)written >= destSize - *offset)
            return FALSE;
        *offset += written;
    }
    return TRUE;
}

static bool32 AppendStringField(char *dest, u32 destSize, u32 *offset,
                                const char *key, const char *value, bool32 comma)
{
    int written = snprintf(dest + *offset, destSize - *offset, "  \"%s\": \"", key);
    if (written < 0 || (u32)written >= destSize - *offset)
        return FALSE;
    *offset += written;
    if (!AppendJsonString(dest, destSize, offset, value))
        return FALSE;
    written = snprintf(dest + *offset, destSize - *offset, "\"%s\n", comma ? "," : "");
    if (written < 0 || (u32)written >= destSize - *offset)
        return FALSE;
    *offset += written;
    return TRUE;
}

bool32 Platform_StateMetadataWrite(u8 slot, const struct PlatformStateMetadata *metadata)
{
    char path[1024];
    char json[2048];
    u32 offset;
    int written;

    if (metadata == NULL
     || !Platform_ProfileGetStateFilePath(slot, PLATFORM_STATE_FILE_METADATA, path, sizeof(path)))
        return FALSE;
    written = snprintf(json, sizeof(json), "{\n  \"metadata_version\": %u,\n", metadata->metadataVersion);
    if (written < 0 || (u32)written >= sizeof(json))
        return FALSE;
    offset = written;
    if (!AppendStringField(json, sizeof(json), &offset, "slot", metadata->slotId, TRUE)
     || !AppendStringField(json, sizeof(json), &offset, "profile_id", metadata->profileId, TRUE)
     || !AppendStringField(json, sizeof(json), &offset, "timestamp", metadata->timestamp, TRUE))
        return FALSE;
    written = snprintf(json + offset, sizeof(json) - offset,
                       "  \"play_time_seconds\": %llu,\n",
                       (unsigned long long)metadata->playTimeSeconds);
    if (written < 0 || (u32)written >= sizeof(json) - offset)
        return FALSE;
    offset += written;
    if (!AppendStringField(json, sizeof(json), &offset, "location", metadata->location, TRUE)
     || !AppendStringField(json, sizeof(json), &offset, "context", metadata->context, TRUE))
        return FALSE;
    written = snprintf(json + offset, sizeof(json) - offset,
                       "  \"state_version\": %u\n}\n", metadata->stateVersion);
    if (written < 0 || (u32)written >= sizeof(json) - offset)
        return FALSE;
    offset += written;
    return Platform_StorageWriteAtomic(path, json, offset);
}

static const char *FindField(const char *json, const char *key)
{
    char pattern[64];
    const char *field;
    int length = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (length <= 0 || (u32)length >= sizeof(pattern))
        return NULL;
    field = strstr(json, pattern);
    if (field == NULL)
        return NULL;
    field += length;
    while (isspace((unsigned char)*field)) field++;
    if (*field++ != ':')
        return NULL;
    while (isspace((unsigned char)*field)) field++;
    return field;
}

static bool32 ReadStringField(const char *json, const char *key, char *dest, u32 destSize)
{
    const char *value = FindField(json, key);
    u32 output = 0;
    if (value == NULL || *value++ != '"' || destSize == 0)
        return FALSE;
    while (*value != '\0' && *value != '"' && output + 1 < destSize)
    {
        char c = *value++;
        if (c == '\\')
        {
            c = *value++;
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c != '\\' && c != '"') return FALSE;
        }
        dest[output++] = c;
    }
    if (*value != '"')
        return FALSE;
    dest[output] = '\0';
    return TRUE;
}

static bool32 ReadUnsignedField(const char *json, const char *key, u64 *result)
{
    const char *value = FindField(json, key);
    char *end;
    unsigned long long parsed;
    if (value == NULL || !isdigit((unsigned char)*value))
        return FALSE;
    parsed = strtoull(value, &end, 10);
    if (end == value)
        return FALSE;
    *result = (u64)parsed;
    return TRUE;
}

bool32 Platform_StateMetadataRead(u8 slot, struct PlatformStateMetadata *metadata)
{
    char path[1024];
    char json[STATE_METADATA_FILE_CAPACITY];
    u32 size;
    u64 value;

    if (metadata == NULL
     || !Platform_ProfileGetStateFilePath(slot, PLATFORM_STATE_FILE_METADATA, path, sizeof(path))
     || !Platform_StorageReadFile(path, json, sizeof(json) - 1, &size))
        return FALSE;
    json[size] = '\0';
    memset(metadata, 0, sizeof(*metadata));
    if (!ReadUnsignedField(json, "metadata_version", &value))
        return FALSE;
    metadata->metadataVersion = (u32)value;
    if (metadata->metadataVersion != 1
     || !ReadStringField(json, "slot", metadata->slotId, sizeof(metadata->slotId))
     || !ReadStringField(json, "profile_id", metadata->profileId, sizeof(metadata->profileId))
     || !ReadStringField(json, "timestamp", metadata->timestamp, sizeof(metadata->timestamp))
     || !ReadUnsignedField(json, "play_time_seconds", &metadata->playTimeSeconds)
     || !ReadStringField(json, "location", metadata->location, sizeof(metadata->location))
     || !ReadStringField(json, "context", metadata->context, sizeof(metadata->context))
     || !ReadUnsignedField(json, "state_version", &value))
        return FALSE;
    metadata->stateVersion = (u32)value;
    return TRUE;
}

void Platform_StateMetadataFormatPlayTime(const struct PlatformStateMetadata *metadata,
                                          char *dest, u32 destSize)
{
    u64 seconds = metadata != NULL ? metadata->playTimeSeconds : 0;
    snprintf(dest, destSize, "%02llu:%02llu:%02llu",
             (unsigned long long)(seconds / 3600),
             (unsigned long long)((seconds / 60) % 60),
             (unsigned long long)(seconds % 60));
}

void Platform_StateMetadataFormatTimestamp(const struct PlatformStateMetadata *metadata,
                                           char *dest, u32 destSize)
{
    static const char *const months[] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int year, month, day, hour, minute, second;
    const char *meridiem;
    int displayHour;

    if (metadata == NULL
     || sscanf(metadata->timestamp, "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6
     || month < 1 || month > 12 || hour < 0 || hour > 23)
    {
        CopyText(dest, destSize, "Unknown time");
        return;
    }
    meridiem = hour >= 12 ? "PM" : "AM";
    displayHour = hour % 12;
    if (displayHour == 0) displayHour = 12;
    snprintf(dest, destSize, "%s %d, %d %d:%02d %s",
             months[month - 1], day, year, displayHour, minute, meridiem);
    (void)second;
}

#endif
