#ifndef GUARD_PLATFORM_DESKTOP_VIDEO_H
#define GUARD_PLATFORM_DESKTOP_VIDEO_H

#include "gba/types.h"
#include "platform.h"

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

extern SDL_Window *sdlWindow;
extern SDL_Renderer *sdlRenderer;
extern SDL_Texture *sdlTexture;

bool32 Platform_VideoInit(void);
void Platform_VideoDrawFrame(void);
void Platform_VideoRenderFramebuffer(void);
void Platform_VideoPresent(void);
void Platform_VideoSetStatus(const char *status);
void Platform_VideoSetFastForward(bool32 active);
bool32 Platform_VideoCopyFramebuffer(void *dest, u32 size);
bool32 Platform_VideoRestoreFramebuffer(const void *source, u32 size);
void Platform_VideoBeginHostUi(void);
void Platform_VideoEndHostUi(void);
void Platform_VideoApplySetting(enum PlatformSetting setting, u8 value);
u8 Platform_VideoGetBackgroundCount(void);
void Platform_VideoShutdown(void);

#endif
