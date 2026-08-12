#ifndef GUARD_PLATFORM_DESKTOP_STORAGE_H
#define GUARD_PLATFORM_DESKTOP_STORAGE_H

#include "gba/types.h"

bool32 Platform_StorageInit(const char *preferredRoot);
bool32 Platform_StorageLoadSave(u8 *image, u32 size);
bool32 Platform_StorageWriteSave(const u8 *image, u32 size);
bool32 Platform_StorageLoadSaveFromPath(const char *path, u8 *image, u32 size);
bool32 Platform_StorageWriteSaveToPath(const char *path, const u8 *image, u32 size);
bool32 Platform_StorageReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size);
bool32 Platform_StorageWriteAtomic(const char *path, const void *data, u32 size);
bool32 Platform_StorageReadFile(const char *path, void *data, u32 capacity, u32 *size);
bool32 Platform_StorageCopyFileAtomic(const char *source, const char *destination);
bool32 Platform_StorageFileExists(const char *path);
bool32 Platform_StorageRemoveFile(const char *path);
bool32 Platform_StorageEnsureDirectory(const char *path);
bool32 Platform_StorageRemoveDirectory(const char *path);
bool32 Platform_StorageSetSavePath(const char *path);
const char *Platform_StorageGetRootPath(void);
const char *Platform_StorageGetConfigPath(void);
const char *Platform_StorageGetLegacyConfigPath(void);
const char *Platform_StorageGetLegacySavePath(void);
const char *Platform_StorageGetPreferredLegacySavePath(void);
const char *Platform_StorageGetActiveSavePath(void);
bool32 Platform_StorageUsesPreferredRoot(void);
bool32 Platform_StorageActiveSaveIsProfile(void);
void Platform_StorageShutdown(void);

#endif
