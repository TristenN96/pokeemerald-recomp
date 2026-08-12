#ifndef GUARD_PLATFORM_DESKTOP_GAME_CONTENT_H
#define GUARD_PLATFORM_DESKTOP_GAME_CONTENT_H

#include "gba/types.h"

#define PLATFORM_GAME_CONTENT_SHA1_LENGTH 40
#define PLATFORM_GAME_CONTENT_FORMAT_VERSION 1

#ifndef EMERALD_EXPECTED_SHA1
#error "EMERALD_EXPECTED_SHA1 must be supplied from the canonical rom.sha1 fingerprint"
#endif

#define EMERALD_SUPPORTED_REVISION "Pokemon Emerald (USA, Europe), Rev 0 (BPEE01)"

enum PlatformGameContentImportResult
{
    PLATFORM_GAME_CONTENT_IMPORT_OK,
    PLATFORM_GAME_CONTENT_IMPORT_UNREADABLE,
    PLATFORM_GAME_CONTENT_IMPORT_UNSUPPORTED_ROM,
    PLATFORM_GAME_CONTENT_IMPORT_INSTALL_FAILED,
};

struct PlatformGameContentImportInfo
{
    enum PlatformGameContentImportResult result;
    char detectedSha1[PLATFORM_GAME_CONTENT_SHA1_LENGTH + 1];
    char error[192];
};

bool32 Platform_GameContentVerifyInstalled(bool32 hydrate);
enum PlatformGameContentImportResult Platform_GameContentImport(
    const char *romPath, struct PlatformGameContentImportInfo *info);
const char *Platform_GameContentGetExpectedSha1(void);
const char *Platform_GameContentGetRevision(void);
const char *Platform_GameContentGetLastError(void);
const char *Platform_GameContentGetInstallPath(void);
const char *Platform_GameContentGetInstalledPackageSha1(void);

#endif
