#ifdef PLATFORM_SDL2

#include <stdio.h>
#include <stdlib.h>

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#if defined(NATIVE_LINUX) || defined(_WIN32)
#include <SDL2/SDL_image.h>
#endif

#include "global.h"
#include "platform.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_state_thumbnail.h"
#include "platform/desktop_storage.h"
#include "platform/desktop_video.h"

bool32 Platform_StateThumbnailCapture(u8 slot)
{
#if defined(NATIVE_LINUX) || defined(_WIN32)
    const u32 framebufferSize = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(u32);
    u32 *framebuffer;
    SDL_Surface *surface;
    char path[1024];
    char temporaryPath[1040];
    bool32 result = FALSE;

    if (!Platform_ProfileGetStateFilePath(slot, PLATFORM_STATE_FILE_THUMBNAIL,
                                          path, sizeof(path))
     || snprintf(temporaryPath, sizeof(temporaryPath), "%s.capture", path) <= 0)
        return FALSE;
    framebuffer = malloc(framebufferSize);
    if (framebuffer == NULL)
        return FALSE;
    if (!Platform_VideoCopyFramebuffer(framebuffer, framebufferSize))
    {
        free(framebuffer);
        return FALSE;
    }
    surface = SDL_CreateRGBSurfaceWithFormatFrom(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                                  32, DISPLAY_WIDTH * sizeof(u32),
                                                  SDL_PIXELFORMAT_ARGB8888);
    if (surface != NULL)
    {
        if (IMG_SavePNG(surface, temporaryPath) == 0)
            result = Platform_StorageCopyFileAtomic(temporaryPath, path);
        SDL_FreeSurface(surface);
    }
    Platform_StorageRemoveFile(temporaryPath);
    free(framebuffer);
    return result;
#else
    (void)slot;
    return FALSE;
#endif
}

SDL_Texture *Platform_StateThumbnailLoad(u8 slot)
{
#if defined(NATIVE_LINUX) || defined(_WIN32)
    char path[1024];
    if (!Platform_ProfileGetStateFilePath(slot, PLATFORM_STATE_FILE_THUMBNAIL,
                                          path, sizeof(path)))
        return NULL;
    return IMG_LoadTexture(sdlRenderer, path);
#else
    (void)slot;
    return NULL;
#endif
}

#endif
