#ifdef PLATFORM_SDL2
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#endif

#ifdef __ANDROID__
#include <jni.h>
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#if defined(NATIVE_LINUX) || defined(_WIN32)
#include <SDL2/SDL_image.h>
#endif

#include "global.h"
#include "platform.h"
#include "rtc.h"
#include "gba/defines.h"
#include "gba/m4a_internal.h"
#include "cgb_audio.h"
#include "gba/flash_internal.h"
#include "platform/dma.h"
#include "platform/framedraw.h"
#include "platform/desktop_config.h"
#include "platform/desktop_clock.h"
#include "platform/desktop_audio.h"
#include "platform/desktop_input.h"
#include "platform/desktop_scheduler.h"
#include "platform/desktop_video.h"
#include "platform/desktop_storage.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_frontend.h"
#include "platform/desktop_game_content.h"
#include "platform/desktop_state.h"
#include "platform/desktop_state_ui.h"
#include "platform/native_state.h"

HOST_DATA bool speedUp = false;
HOST_DATA bool isRunning = true;
HOST_DATA bool paused = false;
HOST_DATA double simTime = 0;
HOST_DATA double lastGameTime = 0;
HOST_DATA double curGameTime = 0;
HOST_DATA double fixedTimestep = 1.0 / 60.0; // 16.666667ms
HOST_DATA double timeScale = 1.0;

extern void DoSoftReset(void);

enum NativeStateRequest
{
    NATIVE_STATE_REQUEST_NONE,
    NATIVE_STATE_REQUEST_SAVE,
    NATIVE_STATE_REQUEST_LOAD,
};

static void PrepareHostFrame(enum NativeStateRequest request, u8 slot,
                             double *accumulator, u64 *lastPresentationCounter)
{
    enum NativeStateResult result;
    const char *reason;
    char status[256];

    if (request == NATIVE_STATE_REQUEST_LOAD)
    {
        result = Platform_StateLoad(slot) == PLATFORM_STATE_OPERATION_OK
            ? NATIVE_STATE_OK : NATIVE_STATE_UNAVAILABLE;
        if (result == NATIVE_STATE_OK)
        {
            char path[1024];
            Platform_VideoSetStatus("State loaded");
            DBGPRINTF("Native state loaded (slot %u)\n", slot);
            if (!Platform_ProfileGetStatePath(slot, path, sizeof(path)))
                snprintf(path, sizeof(path), "<unknown>");
            fprintf(stderr, "Native state loaded (slot %u): %s\n", slot, path);
            fflush(stderr);
            *accumulator = 0.0;
            *lastPresentationCounter = 0;
            return;
        }
        reason = Platform_StateGetLastError();
        if (reason == NULL || reason[0] == '\0')
            reason = "unknown serializer error";
        snprintf(status, sizeof(status), "State load failed: %s", reason);
        Platform_VideoSetStatus(status);
        fprintf(stderr, "Native state load failed (slot %u): %s\n", slot, reason);
        fflush(stderr);
        DBGPRINTF("Native state load failed (slot %u): %s\n", slot, reason);
    }

    Platform_VideoDrawFrame();
    if (request == NATIVE_STATE_REQUEST_SAVE)
    {
        {
            enum PlatformStateOperationResult operationResult = Platform_StateSave(slot);
            result = operationResult == PLATFORM_STATE_OPERATION_FAILED
                ? NATIVE_STATE_UNAVAILABLE : NATIVE_STATE_OK;
        }
        if (result == NATIVE_STATE_OK)
        {
            char path[1024];
            reason = Platform_StateGetLastError();
            Platform_VideoSetStatus(reason != NULL && reason[0] != '\0'
                                  ? reason : "State saved");
            DBGPRINTF("Native state saved (slot %u)\n", slot);
            if (!Platform_ProfileGetStatePath(slot, path, sizeof(path)))
                snprintf(path, sizeof(path), "<unknown>");
            fprintf(stderr, "Native state saved (slot %u): %s\n", slot, path);
            fflush(stderr);
        }
        else
        {
            reason = Platform_StateGetLastError();
            if (reason == NULL || reason[0] == '\0')
                reason = "unknown serializer error";
            snprintf(status, sizeof(status), "State save failed: %s", reason);
            Platform_VideoSetStatus(status);
            fprintf(stderr, "Native state save failed (slot %u): %s\n", slot, reason);
            fflush(stderr);
            DBGPRINTF("Native state save failed (slot %u): %s\n", slot, reason);
        }
    }
}

static enum PlatformStateUiResult RunStateUi(bool32 manager,
                                              double *accumulator,
                                              u64 *lastPresentationCounter)
{
    bool32 wasPaused = paused;
    enum PlatformStateUiResult result;

    if (!Platform_SchedulerWaitForFrame(1000))
    {
        Platform_VideoSetStatus("Unable to pause game for save-state menu");
        return PLATFORM_STATE_UI_CLOSED;
    }
    /* The worker is blocked at VBlank here. Draw and retain that exact logical
     * frame before the host UI replaces the renderer output. */
    Platform_VideoDrawFrame();
    paused = TRUE;
    Platform_AudioSetPaused(TRUE);
    result = manager ? Platform_StateUiRunManager() : Platform_StateUiRunSavePicker();
    Platform_VideoRenderFramebuffer();
    paused = wasPaused;
    if (!paused)
    {
        Platform_AudioClearQueue();
        Platform_AudioSetPaused(FALSE);
    }
    else
        Platform_AudioSetPaused(TRUE);
    *accumulator = 0.0;
    *lastPresentationCounter = 0;
    return result;
}

static bool32 EnvironmentEnabled(const char *name)
{
    const char *value = getenv(name);
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static u32 EnvironmentUnsigned(const char *name, u32 fallback)
{
    const char *value = getenv(name);
    char *end;
    unsigned long parsed;
    if (value == NULL || value[0] == '\0')
        return fallback;
    parsed = strtoul(value, &end, 10);
    if (*end != '\0' || parsed > UINT32_MAX)
        return fallback;
    return (u32)parsed;
}

static void PacePresentation(u64 *deadline)
{
    u64 frequency = SDL_GetPerformanceFrequency();
    u64 interval;
    u64 now;
    if (frequency == 0)
        return;
    interval = frequency / 60;
    now = SDL_GetPerformanceCounter();
    if (*deadline == 0 || now > *deadline + interval * 4)
        *deadline = now;
    *deadline += interval;
    while ((now = SDL_GetPerformanceCounter()) < *deadline)
    {
        u64 remaining = *deadline - now;
        u32 delay = (u32)(remaining * 1000 / frequency);
        SDL_Delay(delay > 1 ? delay - 1 : 0);
    }
}

#ifdef __ANDROID__
void Platform_HandleTouchEvent(const SDL_TouchFingerEvent *event);
static void DrawTouchControls(void);
#endif

int main(int argc, char **argv)
{
    const char *importRomPath = NULL;
    const char *dataRootOverride = NULL;
    bool32 verifyGameData = FALSE;
    bool32 printDataPath = FALSE;
    bool32 utilityMode;
    char *prefPath = NULL;
    int argIndex;
#if defined(_WIN32) && defined(WINDOWS_DEBUG_CONSOLE)
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    for (argIndex = 1; argIndex < argc; argIndex++)
    {
        if (strcmp(argv[argIndex], "--import-rom") == 0 && argIndex + 1 < argc)
            importRomPath = argv[++argIndex];
        else if (strcmp(argv[argIndex], "--data-root") == 0 && argIndex + 1 < argc)
            dataRootOverride = argv[++argIndex];
        else if (strcmp(argv[argIndex], "--verify-game-data") == 0)
            verifyGameData = TRUE;
        else if (strcmp(argv[argIndex], "--print-data-path") == 0)
            printDataPath = TRUE;
        else
        {
            fprintf(stderr, "Usage: %s [--data-root PATH] [--import-rom FILE] "
                            "[--verify-game-data] [--print-data-path]\n", argv[0]);
            return 2;
        }
    }
    if (dataRootOverride == NULL || dataRootOverride[0] == '\0')
        dataRootOverride = getenv("POKEEMERALD_DATA_ROOT");
    utilityMode = importRomPath != NULL || verifyGameData || printDataPath;

#ifdef __ANDROID__
    SDL_setenv("SDL_AUDIODRIVER", "openslES", 1);
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#endif
    if(SDL_Init(utilityMode ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER)) < 0)
    {
        DBGPRINTF("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!utilityMode)
        Platform_InputInit();

    if (dataRootOverride == NULL || dataRootOverride[0] == '\0')
    {
#ifdef _WIN32
        prefPath = SDL_GetPrefPath(NULL, "PokemonEmeraldRecomp");
#else
        prefPath = SDL_GetPrefPath("pokeemerald", "pokeemerald");
#endif
    }
    Platform_StorageInit(dataRootOverride != NULL && dataRootOverride[0] != '\0'
                       ? dataRootOverride : prefPath);
    if (!utilityMode)
    {
        Platform_ConfigLoad();
        Platform_ClockInit();
    }
    if (prefPath != NULL)
        SDL_free(prefPath);

    if (printDataPath)
    {
        Platform_GameContentVerifyInstalled(FALSE);
        printf("%s\n", Platform_GameContentGetInstallPath());
        if (importRomPath == NULL && !verifyGameData)
        {
            Platform_StorageShutdown();
            SDL_Quit();
            return 0;
        }
    }
    if (importRomPath != NULL)
    {
        struct PlatformGameContentImportInfo info;
        enum PlatformGameContentImportResult result = Platform_GameContentImport(importRomPath, &info);
        if (result == PLATFORM_GAME_CONTENT_IMPORT_UNSUPPORTED_ROM)
        {
            fprintf(stderr,
                    "Unsupported Pokemon Emerald ROM\n\n"
                    "The selected ROM does not match the supported\n"
                    "Pokemon Emerald revision.\n\n"
                    "Detected SHA-1:\n%s\n\nExpected:\n%s\n",
                    info.detectedSha1, Platform_GameContentGetExpectedSha1());
        }
        else if (result != PLATFORM_GAME_CONTENT_IMPORT_OK)
            fprintf(stderr, "ROM import failed: %s\n", info.error);
        else
        {
            printf("Installed Pokemon Emerald content at %s\n",
                   Platform_GameContentGetInstallPath());
            printf("Source SHA-1: %s\n", info.detectedSha1);
            printf("Package SHA-1: %s\n", Platform_GameContentGetInstalledPackageSha1());
        }
        Platform_StorageShutdown();
        SDL_Quit();
        return result == PLATFORM_GAME_CONTENT_IMPORT_OK ? 0 : 2;
    }
    if (verifyGameData)
    {
        bool32 valid = Platform_GameContentVerifyInstalled(TRUE);
        if (valid)
            printf("Pokemon Emerald content verified: %s\n",
                   Platform_GameContentGetInstalledPackageSha1());
        else
            fprintf(stderr, "Pokemon Emerald content invalid: %s\n",
                    Platform_GameContentGetLastError());
        Platform_StorageShutdown();
        SDL_Quit();
        return valid ? 0 : 2;
    }

    if (!Platform_VideoInit())
        return 1;

    if (!Platform_GameContentVerifyInstalled(TRUE)
     && !Platform_FrontendRunGameDataSetup())
    {
        Platform_VideoShutdown();
        Platform_InputShutdown();
        Platform_StorageShutdown();
        SDL_Quit();
        return 0;
    }

    if (!Platform_ProfileInit())
        return 1;

    if (!Platform_FrontendRunStartup())
    {
        Platform_VideoShutdown();
        Platform_InputShutdown();
        Platform_StorageShutdown();
        SDL_Quit();
        return 0;
    }
    Platform_ProfileLoadSelectedSave(FLASH_BASE, sizeof(FLASH_BASE));
    {
        const struct PlatformProfileMetadata *profile = Platform_ProfileGetSelected();
        fprintf(stderr,
                "Platform startup: profile=%s save=%s profile_storage=%s legacy_fallback=%s\n",
                profile != NULL ? profile->id : "<none>",
                Platform_StorageGetActiveSavePath(),
                Platform_StorageActiveSaveIsProfile() ? "yes" : "no",
                Platform_StorageActiveSaveIsProfile() ? "disabled" : "enabled");
        fflush(stderr);
    }

#if 0
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
#if defined(NATIVE_LINUX) || defined(_WIN32)
    sdlWindow = SDL_CreateWindow("Pokemon Emerald", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#else
    sdlWindow = SDL_CreateWindow("pokeemerald", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH * videoScale, DISPLAY_HEIGHT * videoScale, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif
    if (sdlWindow == NULL)
    {
        DBGPRINTF("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __ANDROID__
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
#else
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC);
#endif
    if (sdlRenderer == NULL)
    {
        DBGPRINTF("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    for (int i = 1; i < 15; i++)
    {
        char filename[16];
#ifdef _WIN32
        snprintf(filename, sizeof(filename), "BG%d.bmp", i);
#else
        snprintf(filename, sizeof(filename), "BG%d.png", i);
#endif
        SDL_RWops *backgroundFile = SDL_RWFromFile(filename, "rb");
        if (backgroundFile == NULL)
            break;
        SDL_RWclose(backgroundFile);
        sBorderBackgroundCount++;
    }
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
#ifdef NATIVE_LINUX
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
            snprintf(filename, sizeof(filename), i == 0 ? "BG.png" : "BG%d.png", i);
            sdlBackgroundTextures[i] = IMG_LoadTexture(sdlRenderer, filename);
        }
        sdlBorderTexture = IMG_LoadTexture(sdlRenderer, "Border.png");
        if (sdlBackgroundTextures[0] == NULL)
            SDL_Log("Background image could not be loaded: %s", IMG_GetError());
        if (sdlBorderTexture == NULL)
            SDL_Log("Border image could not be loaded: %s", IMG_GetError());
    }
#elif defined(_WIN32)
    SDL_RenderSetLogicalSize(sdlRenderer, 0, 0);
    SDL_Surface *borderSurface = SDL_LoadBMP("Border.bmp");
    for (int i = 0; i < sBorderBackgroundCount; i++)
    {
        char filename[16];
        snprintf(filename, sizeof(filename), i == 0 ? "BG.bmp" : "BG%d.bmp", i);
        SDL_Surface *backgroundSurface = SDL_LoadBMP(filename);
        if (backgroundSurface == NULL)
            continue;
        sdlBackgroundTextures[i] = SDL_CreateTextureFromSurface(sdlRenderer, backgroundSurface);
        SDL_FreeSurface(backgroundSurface);
    }
    if (sdlBackgroundTextures[0] == NULL)
        SDL_Log("Background image could not be loaded: %s", SDL_GetError());
    if (borderSurface == NULL)
    {
        SDL_Log("Border image could not be loaded: %s", SDL_GetError());
    }
    else
    {
        sdlBorderTexture = SDL_CreateTextureFromSurface(sdlRenderer, borderSurface);
        SDL_FreeSurface(borderSurface);
    }
#else
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
#endif
    ApplyPlatformSettings();

    sdlTexture = SDL_CreateTexture(sdlRenderer,
                                   SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (sdlTexture == NULL)
    {
        DBGPRINTF("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_NONE);
#endif

    simTime = curGameTime = lastGameTime = SDL_GetPerformanceCounter();

    Platform_AudioInit(42060);
    cgb_audio_init(42060);
#ifndef __ANDROID__
    Platform_VideoDrawFrame();
#endif
    if (!Platform_SchedulerInit())
    {
        DBGPRINTF("Unable to initialize scheduler\n");
        return 1;
    }

    double accumulator = 0.0;
    u64 lastPresentationCounter = 0;
    u64 presentationCount = 0;
    enum NativeStateRequest stateRequest = NATIVE_STATE_REQUEST_NONE;
    bool32 traceScheduler = EnvironmentEnabled("POKEEMERALD_SCHEDULER_TRACE");
    u32 probePresentations = EnvironmentUnsigned("POKEEMERALD_SCHEDULER_PROBE_PRESENTATIONS", 0);
    u32 probeSpeed = EnvironmentUnsigned("POKEEMERALD_SCHEDULER_PROBE_SPEED", UINT32_MAX);

    if (probeSpeed == 0 || (probeSpeed >= 1 && probeSpeed <= 5))
    {
        speedUp = probeSpeed != 1;
        timeScale = probeSpeed == 0 ? 0.0 : probeSpeed;
        Platform_SchedulerSetSpeed((u8)probeSpeed);
        Platform_VideoSetFastForward(speedUp);
    }

    while (isRunning)
    {
        struct PlatformInputActions input;
        Platform_InputPoll(&input);
        if (input.quit)
            isRunning = false;
        if (input.quickSave)
            stateRequest = NATIVE_STATE_REQUEST_SAVE;
        if (input.quickLoad)
            stateRequest = NATIVE_STATE_REQUEST_LOAD;
        if (input.openStateManager)
        {
            enum PlatformStateUiResult uiResult = RunStateUi(TRUE, &accumulator,
                                                              &lastPresentationCounter);
            if (uiResult == PLATFORM_STATE_UI_LOADED)
                Platform_VideoSetStatus("State loaded");
            else if (uiResult == PLATFORM_STATE_UI_QUIT)
                isRunning = FALSE;
        }
        if (input.manualSave && isRunning)
        {
            enum PlatformStateUiResult uiResult = RunStateUi(FALSE, &accumulator,
                                                              &lastPresentationCounter);
            if (uiResult == PLATFORM_STATE_UI_SAVED)
            {
                const char *warning = Platform_StateGetLastError();
                Platform_VideoSetStatus(warning != NULL && warning[0] != '\0'
                                      ? warning : "State saved");
            }
            else if (uiResult == PLATFORM_STATE_UI_QUIT)
                isRunning = FALSE;
        }
        if (input.reset)
            DoSoftReset();
        if (input.openSettings)
        {
            bool32 wasPaused = paused;
            paused = TRUE;
            Platform_AudioSetPaused(TRUE);
            Platform_FrontendRunSettings();
            paused = wasPaused;
            if (!paused)
            {
                Platform_AudioClearQueue();
                Platform_AudioSetPaused(FALSE);
            }
            lastPresentationCounter = 0;
            accumulator = 0.0;
        }
        if (input.togglePause)
        {
            paused = !paused;
            if (paused)
                Platform_AudioSetPaused(TRUE);
            else
            {
                Platform_AudioClearQueue();
                Platform_AudioSetPaused(FALSE);
            }
        }
        if (input.speedUpChanged)
        {
            speedUp = input.speedUp;
            timeScale = speedUp ? (input.speed == 0 ? 0.0 : input.speed) : 1.0;
            Platform_SchedulerSetSpeed(speedUp ? input.speed : 1);
            Platform_VideoSetFastForward(speedUp);
            lastPresentationCounter = 0;
            if (!speedUp && paused)
                Platform_AudioSetPaused(TRUE);
        }

        /* A key request is not considered serviced until the worker has
         * published the next quiescent VBlank boundary. */
        if (stateRequest != NATIVE_STATE_REQUEST_NONE
         && Platform_SchedulerWaitForFrame(1000))
        {
            PrepareHostFrame(stateRequest, PLATFORM_STATE_QUICK_SLOT,
                             &accumulator, &lastPresentationCounter);
            stateRequest = NATIVE_STATE_REQUEST_NONE;
        }

        if (!paused)
        {
            u8 schedulerSpeed = Platform_SchedulerGetSpeed();
            u64 beforeFrame = Platform_SchedulerGetFrameCounter();
            u64 frequency = SDL_GetPerformanceFrequency();
            u64 batchDeadline;
            u32 targetFrames = schedulerSpeed == 0 ? 256 : schedulerSpeed;
            u32 completedFrames = 0;

            if (lastPresentationCounter == 0)
                lastPresentationCounter = SDL_GetPerformanceCounter();
            batchDeadline = lastPresentationCounter + frequency / 60;
            while (completedFrames < targetFrames && isRunning)
            {
                bool32 finalFrame;
                if (!Platform_SchedulerWaitForFrame(1000))
                {
                    fprintf(stderr, "Scheduler timed out waiting for simulation frame\n");
                    fflush(stderr);
                    isRunning = FALSE;
                    break;
                }
                finalFrame = schedulerSpeed == 0
                    ? ((completedFrames != 0 && SDL_GetPerformanceCounter() >= batchDeadline)
                       || completedFrames + 1 == targetFrames)
                    : completedFrames + 1 == targetFrames;
                if (finalFrame)
                    PrepareHostFrame(NATIVE_STATE_REQUEST_NONE, PLATFORM_STATE_QUICK_SLOT,
                                     &accumulator, &lastPresentationCounter);
                Platform_SchedulerCompleteFrame();
                completedFrames++;
                if (schedulerSpeed == 0 && finalFrame)
                    break;
            }

            if (traceScheduler)
                fprintf(stderr,
                        "Scheduler presentation=%llu speed=%u simulation_frames=%llu\n",
                        (unsigned long long)(presentationCount + 1), schedulerSpeed,
                        (unsigned long long)(Platform_SchedulerGetFrameCounter() - beforeFrame));
        }
        else
            SDL_Delay(1);

#ifdef __ANDROID__
        DrawTouchControls();
#endif
        if (speedUp || !Platform_GetSetting(PLATFORM_SETTING_VSYNC))
            PacePresentation(&lastPresentationCounter);
        Platform_VideoPresent();
        presentationCount++;
        if (probePresentations != 0 && presentationCount >= probePresentations)
            isRunning = FALSE;
    }

    //Platform_StoreSaveFile();
    Platform_SchedulerShutdown();
    Platform_StorageShutdown();

    Platform_VideoShutdown();
    Platform_InputShutdown();
    Platform_AudioShutdown();
    SDL_Quit();
    return 0;
}

#if 0
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
#endif

void Platform_StoreSaveFile(void)
{
    if (!Platform_StorageWriteSave(FLASH_BASE, sizeof(FLASH_BASE)))
        DBGPRINTF("Unable to store save file\n");
}

void Platform_ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size)
{
    if (!Platform_StorageReadFlash(sectorNum, offset, dest, size))
        DBGPRINTF("ReadFlash out of bounds or unavailable\n");
}

u8 Platform_GetBorderBackgroundCount(void)
{
    return Platform_VideoGetBackgroundCount() + 1;
}

u8 Platform_GetBorderBackground(void)
{
    if (Platform_ConfigHasBorderBackground())
        return Platform_ConfigGetBorderBackground();
    if (gSaveBlock2Ptr != NULL)
    {
        u8 legacySelection = gSaveBlock2Ptr->optionsBorderBackground;
        if (legacySelection == 1)
            return Platform_VideoGetBackgroundCount();
        if (legacySelection >= 2)
            return legacySelection - 1;
    }
    return 0;
}

void Platform_SetBorderBackground(u8 selection)
{
    Platform_ConfigSetBorderBackground(selection);
    Platform_ConfigStore();
}

void Platform_SetSetting(enum PlatformSetting setting, u8 value)
{
    Platform_ConfigSetSetting(setting, value);
    Platform_VideoApplySetting(setting, value);
    Platform_ConfigStore();
}

#ifdef __ANDROID__
JNIEXPORT jint JNICALL Java_com_pokeemerald_experimental_GbaControlsView_getBorderBackground(JNIEnv *env, jclass clazz)
{
    return Platform_GetBorderBackground();
}

JNIEXPORT jint JNICALL Java_com_pokeemerald_experimental_GbaControlsView_getPlatformSetting(JNIEnv *env, jclass clazz, jint setting)
{
    if (setting < 0 || setting >= PLATFORM_SETTING_COUNT)
        return 0;
    return Platform_GetSetting(setting);
}
#endif


#ifdef __ANDROID__
#define MAX_TOUCH_FINGERS 10

struct TouchFinger
{
    SDL_FingerID id;
    float x;
    float y;
    bool active;
};

HOST_DATA static struct TouchFinger touchFingers[MAX_TOUCH_FINGERS];
HOST_DATA static u16 touchKeys;
static bool IsInsideRect(int x, int y, SDL_Rect rect)
{
    SDL_Point point = {x, y};
    return SDL_PointInRect(&point, &rect);
}

static int MinInt(int a, int b)
{
    return a < b ? a : b;
}

static int GetControlSideWidth(int windowWidth, int windowHeight)
{
    int sideWidth = (windowWidth - windowHeight * 3 / 2) / 2;
    int minimumWidth = windowWidth * 14 / 100;
    return sideWidth > minimumWidth ? sideWidth : minimumWidth;
}

static void UpdateTouchKeys(void)
{
    int windowWidth;
    int windowHeight;
    SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
    int sideWidth = GetControlSideWidth(windowWidth, windowHeight);
    int buttonSize = MinInt(sideWidth * 2 / 5, windowHeight / 6);
    int dpadUnit = MinInt(sideWidth / 3, windowHeight / 8);
    int dpadX = sideWidth * 2 / 3;
    int dpadY = windowHeight * 7 / 10;
    SDL_Rect dpadUp = {dpadX - dpadUnit / 2, dpadY - dpadUnit * 3 / 2,
                       dpadUnit, dpadUnit};
    SDL_Rect dpadDown = {dpadX - dpadUnit / 2, dpadY + dpadUnit / 2,
                         dpadUnit, dpadUnit};
    SDL_Rect dpadLeft = {dpadX - dpadUnit * 3 / 2, dpadY - dpadUnit / 2,
                         dpadUnit, dpadUnit};
    SDL_Rect dpadRight = {dpadX + dpadUnit / 2, dpadY - dpadUnit / 2,
                          dpadUnit, dpadUnit};
    SDL_Rect aButton = {windowWidth - sideWidth / 4 - buttonSize,
                        windowHeight * 58 / 100, buttonSize, buttonSize};
    SDL_Rect bButton = {windowWidth - sideWidth + sideWidth / 4,
                        windowHeight * 76 / 100, buttonSize, buttonSize};
    SDL_Rect selectButton = {sideWidth / 4, windowHeight / 4,
                             sideWidth / 2, windowHeight / 10};
    SDL_Rect startButton = {windowWidth - sideWidth * 3 / 4, windowHeight / 4,
                            sideWidth / 2, windowHeight / 10};
    SDL_Rect lButton = {sideWidth / 4, windowHeight / 20,
                        sideWidth / 2, windowHeight / 10};
    SDL_Rect rButton = {windowWidth - sideWidth * 3 / 4, windowHeight / 20,
                        sideWidth / 2, windowHeight / 10};

    touchKeys = 0;

    for (int i = 0; i < MAX_TOUCH_FINGERS; i++)
    {
        if (!touchFingers[i].active)
            continue;

        int x = touchFingers[i].x * windowWidth;
        int y = touchFingers[i].y * windowHeight;

        if (IsInsideRect(x, y, dpadUp)) touchKeys |= DPAD_UP;
        if (IsInsideRect(x, y, dpadDown)) touchKeys |= DPAD_DOWN;
        if (IsInsideRect(x, y, dpadLeft)) touchKeys |= DPAD_LEFT;
        if (IsInsideRect(x, y, dpadRight)) touchKeys |= DPAD_RIGHT;

        if (IsInsideRect(x, y, aButton)) touchKeys |= A_BUTTON;
        if (IsInsideRect(x, y, bButton)) touchKeys |= B_BUTTON;
        if (IsInsideRect(x, y, startButton)) touchKeys |= START_BUTTON;
        if (IsInsideRect(x, y, selectButton)) touchKeys |= SELECT_BUTTON;
        if (IsInsideRect(x, y, lButton)) touchKeys |= L_BUTTON;
        if (IsInsideRect(x, y, rButton)) touchKeys |= R_BUTTON;
    }
}

void Platform_HandleTouchEvent(const SDL_TouchFingerEvent *event)
{
    int slot = -1;
    for (int i = 0; i < MAX_TOUCH_FINGERS; i++)
    {
        if (touchFingers[i].active && touchFingers[i].id == event->fingerId)
        {
            slot = i;
            break;
        }
        if (slot < 0 && !touchFingers[i].active)
            slot = i;
    }

    if (slot < 0)
        return;

    if (event->type == SDL_FINGERUP)
    {
        touchFingers[slot].active = false;
    }
    else
    {
        touchFingers[slot].id = event->fingerId;
        touchFingers[slot].x = event->x;
        touchFingers[slot].y = event->y;
        touchFingers[slot].active = true;
    }

    UpdateTouchKeys();
}

static const Uint8 *GetGlyph(char character)
{
    static const Uint8 glyphA[7] = {14, 17, 17, 31, 17, 17, 17};
    static const Uint8 glyphB[7] = {30, 17, 17, 30, 17, 17, 30};
    static const Uint8 glyphC[7] = {15, 16, 16, 16, 16, 16, 15};
    static const Uint8 glyphE[7] = {31, 16, 16, 30, 16, 16, 31};
    static const Uint8 glyphL[7] = {16, 16, 16, 16, 16, 16, 31};
    static const Uint8 glyphR[7] = {30, 17, 17, 30, 20, 18, 17};
    static const Uint8 glyphS[7] = {15, 16, 16, 14, 1, 1, 30};
    static const Uint8 glyphT[7] = {31, 4, 4, 4, 4, 4, 4};

    switch (character)
    {
    case 'A': return glyphA;
    case 'B': return glyphB;
    case 'C': return glyphC;
    case 'E': return glyphE;
    case 'L': return glyphL;
    case 'R': return glyphR;
    case 'S': return glyphS;
    case 'T': return glyphT;
    default:  return NULL;
    }
}

static void DrawControlLabel(SDL_Rect rect, const char *label)
{
    int length = SDL_strlen(label);
    int scale = MinInt(rect.h / 9, rect.w / (length * 6));
    if (scale < 1)
        scale = 1;
    int startX = rect.x + (rect.w - (length * 6 - 1) * scale) / 2;
    int startY = rect.y + (rect.h - 7 * scale) / 2;

    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 230);
    for (int character = 0; character < length; character++)
    {
        const Uint8 *glyph = GetGlyph(label[character]);
        if (glyph == NULL)
            continue;
        for (int row = 0; row < 7; row++)
        {
            for (int column = 0; column < 5; column++)
            {
                if (glyph[row] & (1 << (4 - column)))
                {
                    SDL_Rect pixel = {startX + (character * 6 + column) * scale,
                                      startY + row * scale, scale, scale};
                    SDL_RenderFillRect(sdlRenderer, &pixel);
                }
            }
        }
    }
}

static void DrawControlRect(SDL_Rect rect, bool pressed, const char *label)
{
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, pressed ? 150 : 65);
    SDL_RenderFillRect(sdlRenderer, &rect);
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, pressed ? 230 : 130);
    SDL_RenderDrawRect(sdlRenderer, &rect);
    if (label != NULL)
        DrawControlLabel(rect, label);
}

static void DrawTouchControls(void)
{
    int windowWidth;
    int windowHeight;
    SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
    int sideWidth = GetControlSideWidth(windowWidth, windowHeight);
    int buttonSize = MinInt(sideWidth * 2 / 5, windowHeight / 6);
    int dpadUnit = MinInt(sideWidth / 3, windowHeight / 8);
    int dpadX = sideWidth * 2 / 3;
    int dpadY = windowHeight * 7 / 10;

    SDL_RenderSetLogicalSize(sdlRenderer, 0, 0);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    DrawControlRect((SDL_Rect){dpadX - dpadUnit / 2, dpadY - dpadUnit * 3 / 2,
                               dpadUnit, dpadUnit}, touchKeys & DPAD_UP, NULL);
    DrawControlRect((SDL_Rect){dpadX - dpadUnit / 2, dpadY + dpadUnit / 2,
                               dpadUnit, dpadUnit}, touchKeys & DPAD_DOWN, NULL);
    DrawControlRect((SDL_Rect){dpadX - dpadUnit * 3 / 2, dpadY - dpadUnit / 2,
                               dpadUnit, dpadUnit}, touchKeys & DPAD_LEFT, NULL);
    DrawControlRect((SDL_Rect){dpadX + dpadUnit / 2, dpadY - dpadUnit / 2,
                               dpadUnit, dpadUnit}, touchKeys & DPAD_RIGHT, NULL);
    DrawControlRect((SDL_Rect){windowWidth - sideWidth / 4 - buttonSize,
                               windowHeight * 58 / 100, buttonSize, buttonSize}, touchKeys & A_BUTTON, "A");
    DrawControlRect((SDL_Rect){windowWidth - sideWidth + sideWidth / 4,
                               windowHeight * 76 / 100, buttonSize, buttonSize}, touchKeys & B_BUTTON, "B");
    DrawControlRect((SDL_Rect){windowWidth - sideWidth * 3 / 4, windowHeight / 4,
                               sideWidth / 2, windowHeight / 10}, touchKeys & START_BUTTON, "START");
    DrawControlRect((SDL_Rect){sideWidth / 4, windowHeight / 4,
                               sideWidth / 2, windowHeight / 10}, touchKeys & SELECT_BUTTON, "SELECT");
    DrawControlRect((SDL_Rect){sideWidth / 4, windowHeight / 20,
                               sideWidth / 2, windowHeight / 10}, touchKeys & L_BUTTON, "L");
    DrawControlRect((SDL_Rect){windowWidth - sideWidth * 3 / 4, windowHeight / 20,
                               sideWidth / 2, windowHeight / 10}, touchKeys & R_BUTTON, "R");

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
}

#endif

#if 0
void ProcessEvents(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            isRunning = false;
            break;
#ifdef __ANDROID__
        case SDL_CONTROLLERDEVICEADDED:
            if (androidController == NULL && SDL_IsGameController(event.cdevice.which))
                androidController = SDL_GameControllerOpen(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (androidController != NULL
             && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(androidController)) == event.cdevice.which)
            {
                SDL_GameControllerClose(androidController);
                androidController = NULL;
                controllerKeys = 0;
                controllerAxisKeys = 0;
                controllerAxisX = 0;
                controllerAxisY = 0;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            controllerKeys |= ControllerButtonMask(event.cbutton.button);
            break;
        case SDL_CONTROLLERBUTTONUP:
            controllerKeys &= ~ControllerButtonMask(event.cbutton.button);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                controllerAxisX = event.caxis.value;
            else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                controllerAxisY = event.caxis.value;

            controllerAxisKeys = 0;
            if (controllerAxisX < -16000) controllerAxisKeys |= DPAD_LEFT;
            if (controllerAxisX >  16000) controllerAxisKeys |= DPAD_RIGHT;
            if (controllerAxisY < -16000) controllerAxisKeys |= DPAD_UP;
            if (controllerAxisY >  16000) controllerAxisKeys |= DPAD_DOWN;
            break;
#endif
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            HANDLE_KEYUP(A_BUTTON)
            HANDLE_KEYUP(B_BUTTON)
            HANDLE_KEYUP(START_BUTTON)
            HANDLE_KEYUP(SELECT_BUTTON)
            HANDLE_KEYUP(L_BUTTON)
            HANDLE_KEYUP(R_BUTTON)
            HANDLE_KEYUP(DPAD_UP)
            HANDLE_KEYUP(DPAD_DOWN)
            HANDLE_KEYUP(DPAD_LEFT)
            HANDLE_KEYUP(DPAD_RIGHT)
            case SDLK_SPACE:
                if (speedUp)
                {
                    speedUp = false;
                    timeScale = 1.0;
                    Platform_AudioClearQueue();
                    Platform_AudioSetPaused(FALSE);
                }
                break;
            }
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            HANDLE_KEYDOWN(A_BUTTON)
            HANDLE_KEYDOWN(B_BUTTON)
            HANDLE_KEYDOWN(START_BUTTON)
            HANDLE_KEYDOWN(SELECT_BUTTON)
            HANDLE_KEYDOWN(L_BUTTON)
            HANDLE_KEYDOWN(R_BUTTON)
            HANDLE_KEYDOWN(DPAD_UP)
            HANDLE_KEYDOWN(DPAD_DOWN)
            HANDLE_KEYDOWN(DPAD_LEFT)
            HANDLE_KEYDOWN(DPAD_RIGHT)
            case SDLK_r:
                if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
                {
                    DoSoftReset();
                }
                break;
            case SDLK_p:
                if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
                {
                    paused = !paused;
                }
                break;
            case SDLK_SPACE:
                if (!speedUp)
                {
                    speedUp = true;
                    timeScale = 5.0;
                    Platform_AudioSetPaused(TRUE);
                }
                break;
            }
            break;
        }
    }
}

#ifdef _WIN32
#define STICK_THRESHOLD 0.5f
u16 GetXInputKeys()
{
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    DWORD dwResult = XInputGetState(0, &state);
    u16 xinputKeys = 0;

    if (dwResult == ERROR_SUCCESS)
    {
        /* A */      xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) >> 12;
        /* B */      xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) >> 13;
        /* Start */  xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_START) >> 1;
        /* Select */ xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) >> 3;
        /* L */      xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) << 1;
        /* R */      xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) >> 1;
        /* Up */     xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) << 6;
        /* Down */   xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) << 6;
        /* Left */   xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) << 3;
        /* Right */  xinputKeys |= (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) << 1;


        /* Control Stick */
        float xAxis = (float)state.Gamepad.sThumbLX / (float)SHRT_MAX;
        float yAxis = (float)state.Gamepad.sThumbLY / (float)SHRT_MAX;

        if (xAxis < -STICK_THRESHOLD) xinputKeys |= DPAD_LEFT;
        if (xAxis >  STICK_THRESHOLD) xinputKeys |= DPAD_RIGHT;
        if (yAxis < -STICK_THRESHOLD) xinputKeys |= DPAD_DOWN;
        if (yAxis >  STICK_THRESHOLD) xinputKeys |= DPAD_UP;


        /* Speedup */
        // Note: 'speedup' variable is only (un)set on keyboard input
        double oldTimeScale = timeScale;
        timeScale = (state.Gamepad.bRightTrigger > 0x80 || speedUp) ? 5.0 : 1.0;

        if (oldTimeScale != timeScale)
        {
            if (timeScale > 1.0)
            {
                Platform_AudioSetPaused(TRUE);
            }
            else
            {
                Platform_AudioClearQueue();
                Platform_AudioSetPaused(FALSE);
            }
        }
    }

    return xinputKeys;
}
#endif // _WIN32

#endif

u16 Platform_GetKeyInput(void)
{
#ifdef __ANDROID__
    return Platform_InputGetKeys() | touchKeys;
#else
    return Platform_InputGetKeys();
#endif
}

void SoftReset(u32 resetFlags)
{
    puts("Soft Reset called. Exiting.");
    exit(0);
}

#endif
