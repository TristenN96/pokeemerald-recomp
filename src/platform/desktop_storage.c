#ifdef PLATFORM_SDL2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "gba/flash_internal.h"
#include "platform/desktop_filesystem.h"
#include "platform/desktop_storage.h"

#define PLATFORM_STORAGE_PATH_MAX 1024

HOST_DATA static char sRootPath[PLATFORM_STORAGE_PATH_MAX] = ".";
HOST_DATA static char sSavePath[PLATFORM_STORAGE_PATH_MAX] = "pokeemerald.sav";
HOST_DATA static char sConfigPath[PLATFORM_STORAGE_PATH_MAX] = "pokeemerald.cfg";
HOST_DATA static char sPreferredLegacySavePath[PLATFORM_STORAGE_PATH_MAX] = "pokeemerald.sav";
HOST_DATA static char sActiveSavePath[PLATFORM_STORAGE_PATH_MAX] = "pokeemerald.sav";
HOST_DATA static bool32 sUsesPreferredRoot;
HOST_DATA static bool32 sActiveSaveIsProfile;

static bool32 JoinPath(char *dest, u32 destSize, const char *root, const char *name)
{
    size_t rootLength;
    size_t nameLength;
    bool32 hasSeparator;

    if (root == NULL || name == NULL)
        return FALSE;
    rootLength = strlen(root);
    nameLength = strlen(name);
    hasSeparator = rootLength != 0
                && (root[rootLength - 1] == '/' || root[rootLength - 1] == '\\');
    if (rootLength + nameLength + (hasSeparator ? 0 : 1) + 1 > destSize)
        return FALSE;
    memcpy(dest, root, rootLength);
    if (!hasSeparator)
        dest[rootLength++] = '/';
    memcpy(dest + rootLength, name, nameLength + 1);
    return TRUE;
}

static bool32 FileExists(const char *path)
{
    FILE *file = Platform_FileOpen(path, "rb");
    if (file == NULL)
        return FALSE;
    fclose(file);
    return TRUE;
}

bool32 Platform_StorageInit(const char *preferredRoot)
{
    sUsesPreferredRoot = FALSE;
    sActiveSaveIsProfile = FALSE;
    strcpy(sRootPath, ".");
    strcpy(sSavePath, "pokeemerald.sav");
    strcpy(sConfigPath, "pokeemerald.cfg");
    strcpy(sPreferredLegacySavePath, "pokeemerald.sav");
    strcpy(sActiveSavePath, "pokeemerald.sav");

    if (preferredRoot != NULL
     && JoinPath(sRootPath, sizeof(sRootPath), preferredRoot, "")
     && JoinPath(sSavePath, sizeof(sSavePath), preferredRoot, "pokeemerald.sav")
     && JoinPath(sConfigPath, sizeof(sConfigPath), preferredRoot, "pokeemerald.cfg"))
    {
        size_t rootLength = strlen(sRootPath);
        while (rootLength > 1
            && (sRootPath[rootLength - 1] == '/' || sRootPath[rootLength - 1] == '\\'))
            sRootPath[--rootLength] = '\0';
        strcpy(sPreferredLegacySavePath, sSavePath);
        strcpy(sActiveSavePath, sSavePath);
        sUsesPreferredRoot = TRUE;
    }
    return sUsesPreferredRoot;
}

bool32 Platform_StorageFileExists(const char *path)
{
    return path != NULL && FileExists(path);
}

bool32 Platform_StorageLoadSaveFromPath(const char *path, u8 *image, u32 size)
{
    FILE *file;
    long fileSize;
    size_t bytesToRead;
    size_t bytesRead;

    if (path == NULL || image == NULL || size != sizeof(FLASH_BASE))
        return FALSE;
    memset(image, 0xFF, size);
    file = Platform_FileOpen(path, "rb");
    if (file == NULL)
        return FALSE;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return FALSE;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return FALSE;
    }
    bytesToRead = fileSize < (long)size ? (size_t)fileSize : size;
    bytesRead = fread(image, 1, bytesToRead, file);
    if (bytesRead != bytesToRead && ferror(file))
    {
        fclose(file);
        return FALSE;
    }
    fclose(file);
    return TRUE;
}

bool32 Platform_StorageLoadSave(u8 *image, u32 size)
{
    if (Platform_StorageLoadSaveFromPath(sActiveSavePath, image, size))
        return TRUE;
    if (!sActiveSaveIsProfile && sUsesPreferredRoot
     && Platform_StorageLoadSaveFromPath("pokeemerald.sav", image, size))
    {
        if (!FileExists(sActiveSavePath))
            Platform_StorageCopyFileAtomic("pokeemerald.sav", sActiveSavePath);
        return TRUE;
    }
    if (image != NULL && size == sizeof(FLASH_BASE))
        memset(image, 0xFF, size);
    return FALSE;
}

bool32 Platform_StorageWriteAtomic(const char *path, const void *data, u32 size)
{
    char tempPath[PLATFORM_STORAGE_PATH_MAX];
    FILE *file;
    size_t written;

    if (path == NULL || data == NULL || size == 0 || strlen(path) + 5 >= sizeof(tempPath))
        return FALSE;
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", path);
    file = Platform_FileOpen(tempPath, "wb");
    if (file == NULL)
        return FALSE;
    written = fwrite(data, 1, size, file);
    if (written != size || fflush(file) != 0)
    {
        fclose(file);
        Platform_FileRemove(tempPath);
        return FALSE;
    }
    if (fclose(file) != 0 || !Platform_FileReplace(tempPath, path))
    {
        Platform_FileRemove(tempPath);
        return FALSE;
    }
    return TRUE;
}

bool32 Platform_StorageReadFile(const char *path, void *data, u32 capacity, u32 *size)
{
    FILE *file;
    long fileSize;
    size_t bytesRead;

    if (path == NULL || data == NULL || capacity == 0)
        return FALSE;
    file = Platform_FileOpen(path, "rb");
    if (file == NULL)
        return FALSE;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return FALSE;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || (unsigned long)fileSize > capacity || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return FALSE;
    }
    bytesRead = fread(data, 1, (size_t)fileSize, file);
    if (bytesRead != (size_t)fileSize || ferror(file) || fclose(file) != 0)
        return FALSE;
    if (size != NULL)
        *size = (u32)bytesRead;
    return TRUE;
}

bool32 Platform_StorageWriteSaveToPath(const char *path, const u8 *image, u32 size)
{
    return path != NULL && image != NULL && size == sizeof(FLASH_BASE)
        && Platform_StorageWriteAtomic(path, image, size);
}

bool32 Platform_StorageWriteSave(const u8 *image, u32 size)
{
    if (Platform_StorageWriteSaveToPath(sActiveSavePath, image, size))
        return TRUE;
    if (!sActiveSaveIsProfile && sUsesPreferredRoot)
        return Platform_StorageWriteSaveToPath("pokeemerald.sav", image, size);
    return FALSE;
}

bool32 Platform_StorageCopyFileAtomic(const char *source, const char *destination)
{
    char tempPath[PLATFORM_STORAGE_PATH_MAX];
    FILE *input;
    FILE *output;
    unsigned char buffer[8192];
    size_t bytesRead;
    size_t bytesWritten;

    if (source == NULL || destination == NULL || strlen(destination) + 5 >= sizeof(tempPath))
        return FALSE;
    input = Platform_FileOpen(source, "rb");
    if (input == NULL)
        return FALSE;
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", destination);
    output = Platform_FileOpen(tempPath, "wb");
    if (output == NULL)
    {
        fclose(input);
        return FALSE;
    }
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), input)) != 0)
    {
        bytesWritten = fwrite(buffer, 1, bytesRead, output);
        if (bytesWritten != bytesRead)
        {
            fclose(input);
            fclose(output);
            Platform_FileRemove(tempPath);
            return FALSE;
        }
    }
    if (ferror(input) || fflush(output) != 0)
    {
        fclose(input);
        fclose(output);
        Platform_FileRemove(tempPath);
        return FALSE;
    }
    fclose(input);
    if (fclose(output) != 0 || !Platform_FileReplace(tempPath, destination))
    {
        Platform_FileRemove(tempPath);
        return FALSE;
    }
    return TRUE;
}

bool32 Platform_StorageEnsureDirectory(const char *path)
{
    return Platform_DirectoryEnsure(path);
}

bool32 Platform_StorageRemoveFile(const char *path)
{
    return Platform_FileRemove(path);
}

bool32 Platform_StorageRemoveDirectory(const char *path)
{
    return Platform_DirectoryRemove(path);
}

bool32 Platform_StorageSetSavePath(const char *path)
{
    if (path == NULL || strlen(path) >= sizeof(sActiveSavePath))
        return FALSE;
    strcpy(sActiveSavePath, path);
    sActiveSaveIsProfile = TRUE;
    return TRUE;
}

bool32 Platform_StorageReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size)
{
    u32 sectorOffset;
    if (dest == NULL || gFlash == NULL || sectorNum >= gFlash->sector.count)
        return FALSE;
    sectorOffset = ((u32)sectorNum << gFlash->sector.shift);
    if (sectorOffset > sizeof(FLASH_BASE) || offset > sizeof(FLASH_BASE) - sectorOffset)
        return FALSE;
    if (size > sizeof(FLASH_BASE) - sectorOffset - offset)
        return FALSE;
    memcpy(dest, FLASH_BASE + sectorOffset + offset, size);
    return TRUE;
}

const char *Platform_StorageGetRootPath(void)
{
    return sRootPath;
}

const char *Platform_StorageGetConfigPath(void)
{
    return sConfigPath;
}

const char *Platform_StorageGetLegacyConfigPath(void)
{
    return "pokeemerald.cfg";
}

const char *Platform_StorageGetLegacySavePath(void)
{
    return "pokeemerald.sav";
}

const char *Platform_StorageGetPreferredLegacySavePath(void)
{
    return sPreferredLegacySavePath;
}

const char *Platform_StorageGetActiveSavePath(void)
{
    return sActiveSavePath;
}

bool32 Platform_StorageUsesPreferredRoot(void)
{
    return sUsesPreferredRoot;
}

bool32 Platform_StorageActiveSaveIsProfile(void)
{
    return sActiveSaveIsProfile;
}

void Platform_StorageShutdown(void)
{
}

#endif
