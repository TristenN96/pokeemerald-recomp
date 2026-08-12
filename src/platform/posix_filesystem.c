#if defined(PLATFORM_SDL2) && !defined(_WIN32)

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"
#include "platform/desktop_filesystem.h"

FILE *Platform_FileOpen(const char *path, const char *mode)
{
    return path != NULL && mode != NULL ? fopen(path, mode) : NULL;
}

bool32 Platform_FileReplace(const char *source, const char *destination)
{
    return source != NULL && destination != NULL && rename(source, destination) == 0;
}

bool32 Platform_DirectoryEnsure(const char *path)
{
    struct stat info;

    if (path == NULL)
        return FALSE;
    if (stat(path, &info) == 0)
        return S_ISDIR(info.st_mode);
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

bool32 Platform_FileRemove(const char *path)
{
    return path != NULL && (remove(path) == 0 || errno == ENOENT);
}

bool32 Platform_DirectoryRemove(const char *path)
{
    return path != NULL && (rmdir(path) == 0 || errno == ENOENT);
}

#endif
