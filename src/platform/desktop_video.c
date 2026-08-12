#ifdef PLATFORM_SDL2

#include <string.h>

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
#include "platform/framedraw.h"
#include "platform/desktop_assets.h"
#include "platform/desktop_config.h"
#include "platform/desktop_video.h"

HOST_DATA SDL_Window *sdlWindow;
HOST_DATA SDL_Renderer *sdlRenderer;
HOST_DATA SDL_Texture *sdlTexture;

#if defined(NATIVE_LINUX) || defined(_WIN32)
#define MAX_BORDER_BACKGROUNDS 15
HOST_DATA static SDL_Texture *sBackgroundTextures[MAX_BORDER_BACKGROUNDS];
HOST_DATA static SDL_Texture *sBorderTexture;
#endif
HOST_DATA static u8 sBorderBackgroundCount = 1;
HOST_DATA static u32 sFramebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

static void RenderCurrentTexture(void)
{
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
#if defined(NATIVE_LINUX) || defined(_WIN32)
    {
        u8 backgroundOption = Platform_GetBorderBackground();
        int outputWidth;
        int outputHeight;
        int gameHeight;
        int gameWidth;
        SDL_GetRendererOutputSize(sdlRenderer, &outputWidth, &outputHeight);
        if (backgroundOption < sBorderBackgroundCount && sBackgroundTextures[backgroundOption] != NULL)
            SDL_RenderCopy(sdlRenderer, sBackgroundTextures[backgroundOption], NULL, NULL);
        if (Platform_GetSetting(PLATFORM_SETTING_INTEGER_SCALE))
        {
            int scale = outputWidth / DISPLAY_WIDTH;
            if (outputHeight / DISPLAY_HEIGHT < scale)
                scale = outputHeight / DISPLAY_HEIGHT;
            if (scale < 1)
                scale = 1;
            gameWidth = DISPLAY_WIDTH * scale;
            gameHeight = DISPLAY_HEIGHT * scale;
        }
        else
        {
            gameHeight = outputHeight * 8 / 9;
            gameWidth = gameHeight * 3 / 2;
        }
        {
            SDL_Rect gameViewport = {(outputWidth - gameWidth) / 2, (outputHeight - gameHeight) / 2,
                                     gameWidth, gameHeight};
            SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &gameViewport);
            if (Platform_GetSetting(PLATFORM_SETTING_BORDER) && sBorderTexture != NULL)
            {
                SDL_Rect borderSource = {141, 18, 1000, 683};
                int innerWidth = gameViewport.w - 2;
                int innerHeight = gameViewport.h - 2;
                SDL_Rect borderViewport = {
                    gameViewport.x + 1 - innerWidth * 19 / 961,
                    gameViewport.y + 1 - innerHeight * 20 / 643,
                    innerWidth * 1000 / 961,
                    innerHeight * 683 / 643
                };
                SDL_RenderCopy(sdlRenderer, sBorderTexture, &borderSource, &borderViewport);
            }
        }
    }
#else
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
#endif
}

static void ApplyPlatformSettings(void)
{
    SDL_RenderSetVSync(sdlRenderer, Platform_GetSetting(PLATFORM_SETTING_VSYNC));
#if defined(NATIVE_LINUX) || defined(_WIN32)
    SDL_SetWindowFullscreen(sdlWindow, Platform_GetSetting(PLATFORM_SETTING_FULLSCREEN)
                                      ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!Platform_GetSetting(PLATFORM_SETTING_FULLSCREEN))
    {
        int scale = Platform_GetSetting(PLATFORM_SETTING_WINDOW_SCALE);
        SDL_SetWindowSize(sdlWindow, 320 * scale, 180 * scale);
        SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
#endif
}

bool32 Platform_VideoInit(void)
{
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
#if defined(NATIVE_LINUX) || defined(_WIN32)
    sdlWindow = SDL_CreateWindow("Pokemon Emerald", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#else
    sdlWindow = SDL_CreateWindow("pokeemerald", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif
    if (sdlWindow == NULL)
    {
        DBGPRINTF("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return FALSE;
    }

#ifdef __ANDROID__
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
#else
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC);
#endif
    if (sdlRenderer == NULL)
    {
        DBGPRINTF("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return FALSE;
    }
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

#if defined(NATIVE_LINUX) || defined(_WIN32)
    for (int i = 1; i < 15; i++)
    {
        char filename[16];
        char path[1024];
        snprintf(filename, sizeof(filename), "BG%d.png", i);
        if (!Platform_AssetGetPath(filename, path, sizeof(path)))
            break;
        sBorderBackgroundCount++;
    }
#endif
    if (Platform_ConfigGetBackgroundOrderVersion() < 2)
    {
        if (Platform_ConfigHasBorderBackground())
        {
            u8 selection = Platform_ConfigGetBorderBackground();
            if (selection == 1)
                selection = sBorderBackgroundCount;
            else if (selection >= 2)
                selection--;
            Platform_ConfigSetBorderBackground(selection);
        }
        Platform_ConfigSetBackgroundOrderVersion(2);
        Platform_ConfigStore();
    }

#if defined(NATIVE_LINUX) || defined(_WIN32)
    SDL_RenderSetLogicalSize(sdlRenderer, 0, 0);
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
    {
        SDL_Log("SDL_image could not initialize: %s", IMG_GetError());
    }
    else
    {
        for (int i = 0; i < sBorderBackgroundCount; i++)
        {
            char filename[16];
            char path[1024];
            snprintf(filename, sizeof(filename), i == 0 ? "BG.png" : "BG%d.png", i);
            if (Platform_AssetGetPath(filename, path, sizeof(path)))
                sBackgroundTextures[i] = IMG_LoadTexture(sdlRenderer, path);
        }
        {
            char path[1024];
            if (Platform_AssetGetPath("Border.png", path, sizeof(path)))
                sBorderTexture = IMG_LoadTexture(sdlRenderer, path);
        }
        if (sBackgroundTextures[0] == NULL)
            SDL_Log("Background image could not be loaded: %s", IMG_GetError());
        if (sBorderTexture == NULL)
            SDL_Log("Border image could not be loaded: %s", IMG_GetError());
    }
#else
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
#endif
    ApplyPlatformSettings();

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (sdlTexture == NULL)
    {
        DBGPRINTF("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return FALSE;
    }
    SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_NONE);
    return TRUE;
}

void Platform_VideoDrawFrame(void)
{
    static HOST_DATA uint16_t gbaImage[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    memset(gbaImage, 0, sizeof(gbaImage));
    DrawFrame(gbaImage);
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
    {
        uint16_t color = gbaImage[i];
        uint32_t r = (color & 0x1F) * 255 / 31;
        uint32_t g = ((color >> 5) & 0x1F) * 255 / 31;
        uint32_t b = ((color >> 10) & 0x1F) * 255 / 31;
        sFramebuffer[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    SDL_UpdateTexture(sdlTexture, NULL, sFramebuffer, DISPLAY_WIDTH * sizeof(Uint32));
    SDL_RenderClear(sdlRenderer);
#if defined(NATIVE_LINUX) || defined(_WIN32)
    u8 backgroundOption = Platform_GetBorderBackground();
    if (backgroundOption < sBorderBackgroundCount && sBackgroundTextures[backgroundOption] != NULL)
        SDL_RenderCopy(sdlRenderer, sBackgroundTextures[backgroundOption], NULL, NULL);
    {
        int outputWidth;
        int outputHeight;
        int gameHeight;
        int gameWidth;
        SDL_GetRendererOutputSize(sdlRenderer, &outputWidth, &outputHeight);
        if (Platform_GetSetting(PLATFORM_SETTING_INTEGER_SCALE))
        {
            int scale = outputWidth / DISPLAY_WIDTH;
            if (outputHeight / DISPLAY_HEIGHT < scale)
                scale = outputHeight / DISPLAY_HEIGHT;
            if (scale < 1)
                scale = 1;
            gameWidth = DISPLAY_WIDTH * scale;
            gameHeight = DISPLAY_HEIGHT * scale;
        }
        else
        {
            gameHeight = outputHeight * 8 / 9;
            gameWidth = gameHeight * 3 / 2;
        }
        SDL_Rect gameViewport = {(outputWidth - gameWidth) / 2, (outputHeight - gameHeight) / 2,
                                 gameWidth, gameHeight};
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &gameViewport);
        if (Platform_GetSetting(PLATFORM_SETTING_BORDER) && sBorderTexture != NULL)
        {
            SDL_Rect borderSource = {141, 18, 1000, 683};
            int innerWidth = gameViewport.w - 2;
            int innerHeight = gameViewport.h - 2;
            SDL_Rect borderViewport = {
                gameViewport.x + 1 - innerWidth * 19 / 961,
                gameViewport.y + 1 - innerHeight * 20 / 643,
                innerWidth * 1000 / 961,
                innerHeight * 683 / 643
            };
            SDL_RenderCopy(sdlRenderer, sBorderTexture, &borderSource, &borderViewport);
        }
    }
#else
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
#endif
    REG_VCOUNT = 161;
}

void Platform_VideoRenderFramebuffer(void)
{
    if (sdlRenderer != NULL && sdlTexture != NULL)
        RenderCurrentTexture();
}

void Platform_VideoPresent(void)
{
    SDL_RenderPresent(sdlRenderer);
}

void Platform_VideoSetStatus(const char *status)
{
    if (sdlWindow != NULL)
        SDL_SetWindowTitle(sdlWindow, status != NULL && status[0] != '\0'
                                      ? status : "Pokemon Emerald");
}

void Platform_VideoSetFastForward(bool32 active)
{
    /* SDL's PRESENTVSYNC renderer blocks inside SDL_RenderPresent. Disable it
     * for the duration of fast-forward so the host presentation cadence cannot
     * become the simulation clock. The host loop presents the latest texture at
     * its own approximately-60 Hz cadence while the worker continues to run. */
    if (sdlRenderer != NULL)
        SDL_RenderSetVSync(sdlRenderer, active ? 0 : Platform_GetSetting(PLATFORM_SETTING_VSYNC));
}

bool32 Platform_VideoCopyFramebuffer(void *dest, u32 size)
{
    if (dest == NULL || size != sizeof(sFramebuffer))
        return FALSE;
    memcpy(dest, sFramebuffer, sizeof(sFramebuffer));
    return TRUE;
}

bool32 Platform_VideoRestoreFramebuffer(const void *source, u32 size)
{
    if (source == NULL || size != sizeof(sFramebuffer) || sdlTexture == NULL)
        return FALSE;
    memcpy(sFramebuffer, source, sizeof(sFramebuffer));
    SDL_UpdateTexture(sdlTexture, NULL, sFramebuffer, DISPLAY_WIDTH * sizeof(Uint32));
    RenderCurrentTexture();
    return TRUE;
}

void Platform_VideoBeginHostUi(void)
{
    SDL_RenderSetLogicalSize(sdlRenderer, 960, 540);
}

void Platform_VideoEndHostUi(void)
{
#if defined(NATIVE_LINUX) || defined(_WIN32)
    SDL_RenderSetLogicalSize(sdlRenderer, 0, 0);
#else
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
#endif
}

void Platform_VideoApplySetting(enum PlatformSetting setting, u8 value)
{
    if (setting == PLATFORM_SETTING_VSYNC)
        SDL_RenderSetVSync(sdlRenderer, value);
#if defined(NATIVE_LINUX) || defined(_WIN32)
    else if (setting == PLATFORM_SETTING_FULLSCREEN)
    {
        SDL_SetWindowFullscreen(sdlWindow, value ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        if (!value)
        {
            int scale = Platform_GetSetting(PLATFORM_SETTING_WINDOW_SCALE);
            SDL_SetWindowSize(sdlWindow, 320 * scale, 180 * scale);
            SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
    else if (setting == PLATFORM_SETTING_WINDOW_SCALE && !Platform_GetSetting(PLATFORM_SETTING_FULLSCREEN))
    {
        SDL_SetWindowSize(sdlWindow, 320 * value, 180 * value);
        SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
#endif
}

u8 Platform_VideoGetBackgroundCount(void)
{
    return sBorderBackgroundCount;
}


void Platform_VideoShutdown(void)
{
#if defined(NATIVE_LINUX) || defined(_WIN32)
    int i;
    for (i = 0; i < sBorderBackgroundCount; i++)
        SDL_DestroyTexture(sBackgroundTextures[i]);
    SDL_DestroyTexture(sBorderTexture);
#endif
#if defined(NATIVE_LINUX) || defined(_WIN32)
    IMG_Quit();
#endif
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);
    sdlTexture = NULL;
    sdlRenderer = NULL;
    sdlWindow = NULL;
}

#endif
