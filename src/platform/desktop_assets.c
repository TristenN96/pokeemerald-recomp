#ifdef PLATFORM_SDL2

#include <stdio.h>

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "global.h"
#include "platform/desktop_assets.h"

static bool32 AssetExists(const char *path)
{
    SDL_RWops *file = SDL_RWFromFile(path, "rb");

    if (file == NULL)
        return FALSE;
    SDL_RWclose(file);
    return TRUE;
}

bool32 Platform_AssetGetPath(const char *relativePath, char *dest, u32 destSize)
{
    char *basePath;
    int written;

    if (relativePath == NULL || dest == NULL || destSize == 0)
        return FALSE;

    basePath = SDL_GetBasePath();
    if (basePath != NULL)
    {
        written = snprintf(dest, destSize, "%s%s", basePath, relativePath);
        SDL_free(basePath);
        if (written > 0 && (u32)written < destSize && AssetExists(dest))
            return TRUE;
    }

    written = snprintf(dest, destSize, "%s", relativePath);
    return written > 0 && (u32)written < destSize && AssetExists(dest);
}

#endif
