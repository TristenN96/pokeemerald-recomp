#if defined(PLATFORM_SDL2) && defined(_WIN32)

#include <stdio.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "global.h"
#include "platform/desktop_filesystem.h"

static wchar_t *Utf8ToWide(const char *text)
{
    int length;
    wchar_t *wide;

    if (text == NULL)
        return NULL;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (length <= 0)
        return NULL;
    wide = malloc((size_t)length * sizeof(*wide));
    if (wide == NULL)
        return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length) == 0)
    {
        free(wide);
        return NULL;
    }
    return wide;
}

FILE *Platform_FileOpen(const char *path, const char *mode)
{
    wchar_t *widePath = Utf8ToWide(path);
    wchar_t *wideMode = Utf8ToWide(mode);
    FILE *file = NULL;

    if (widePath != NULL && wideMode != NULL)
        file = _wfopen(widePath, wideMode);
    free(widePath);
    free(wideMode);
    return file;
}

bool32 Platform_FileReplace(const char *source, const char *destination)
{
    wchar_t *wideSource = Utf8ToWide(source);
    wchar_t *wideDestination = Utf8ToWide(destination);
    bool32 result = FALSE;

    if (wideSource != NULL && wideDestination != NULL)
        result = MoveFileExW(wideSource, wideDestination,
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    free(wideSource);
    free(wideDestination);
    return result;
}

bool32 Platform_DirectoryEnsure(const char *path)
{
    wchar_t *widePath = Utf8ToWide(path);
    DWORD attributes;
    bool32 result = FALSE;

    if (widePath == NULL)
        return FALSE;
    attributes = GetFileAttributesW(widePath);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        result = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    else if (CreateDirectoryW(widePath, NULL) != 0)
        result = TRUE;
    else if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        attributes = GetFileAttributesW(widePath);
        result = attributes != INVALID_FILE_ATTRIBUTES
              && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    free(widePath);
    return result;
}

bool32 Platform_FileRemove(const char *path)
{
    wchar_t *widePath = Utf8ToWide(path);
    bool32 result;

    if (widePath == NULL)
        return FALSE;
    result = DeleteFileW(widePath) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
    free(widePath);
    return result;
}

bool32 Platform_DirectoryRemove(const char *path)
{
    wchar_t *widePath = Utf8ToWide(path);
    bool32 result;

    if (widePath == NULL)
        return FALSE;
    result = RemoveDirectoryW(widePath) != 0 || GetLastError() == ERROR_PATH_NOT_FOUND;
    free(widePath);
    return result;
}

#endif
