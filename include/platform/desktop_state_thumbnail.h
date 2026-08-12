#ifndef GUARD_PLATFORM_DESKTOP_STATE_THUMBNAIL_H
#define GUARD_PLATFORM_DESKTOP_STATE_THUMBNAIL_H

#include "gba/types.h"

typedef struct SDL_Texture SDL_Texture;

bool32 Platform_StateThumbnailCapture(u8 slot);
SDL_Texture *Platform_StateThumbnailLoad(u8 slot);

#endif
