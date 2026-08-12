#ifdef PLATFORM_SDL2

#include <stdio.h>
#include <string.h>

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "global.h"
#include "platform/desktop_config.h"
#include "platform/desktop_input.h"
#include "platform/desktop_profiles.h"
#include "platform/desktop_state.h"
#include "platform/desktop_state_metadata.h"
#include "platform/desktop_state_thumbnail.h"
#include "platform/desktop_state_ui.h"
#include "platform/desktop_ui.h"
#include "platform/desktop_video.h"

#define STATE_ENTRY_COUNT (PLATFORM_STATE_SLOT_COUNT + 1)

enum ConfirmationResult
{
    CONFIRMATION_CANCEL,
    CONFIRMATION_ACCEPT,
    CONFIRMATION_QUIT
};

static u8 ModifiersFromEvent(SDL_Keymod modifiers)
{
    u8 result = 0;
    if (modifiers & KMOD_SHIFT) result |= PLATFORM_INPUT_MODIFIER_SHIFT;
    if (modifiers & KMOD_CTRL) result |= PLATFORM_INPUT_MODIFIER_CTRL;
    if (modifiers & KMOD_ALT) result |= PLATFORM_INPUT_MODIFIER_ALT;
    if (modifiers & KMOD_GUI) result |= PLATFORM_INPUT_MODIFIER_GUI;
    return result;
}

static bool32 KeyMatchesAction(const SDL_KeyboardEvent *event, enum PlatformInputAction action)
{
    struct PlatformInputBinding binding;
    enum PlatformInputKey key;
    Platform_ConfigGetKeyboardBinding(action, &binding);
    key = Platform_InputKeyFromScancode(event->keysym.scancode);
    return Platform_InputBindingMatches(&binding, key, ModifiersFromEvent(event->keysym.mod));
}

static bool32 IsAcceptEvent(const SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERBUTTONDOWN)
        return event->cbutton.button == SDL_CONTROLLER_BUTTON_A;
    return event->type == SDL_KEYDOWN && !event->key.repeat
        && (event->key.keysym.scancode == SDL_SCANCODE_RETURN
         || KeyMatchesAction(&event->key, PLATFORM_INPUT_ACTION_A));
}

static bool32 IsUpEvent(const SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERBUTTONDOWN)
        return event->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP;
    return event->type == SDL_KEYDOWN
        && (event->key.keysym.scancode == SDL_SCANCODE_UP
         || KeyMatchesAction(&event->key, PLATFORM_INPUT_ACTION_DPAD_UP));
}

static bool32 IsDownEvent(const SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERBUTTONDOWN)
        return event->cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    return event->type == SDL_KEYDOWN
        && (event->key.keysym.scancode == SDL_SCANCODE_DOWN
         || KeyMatchesAction(&event->key, PLATFORM_INPUT_ACTION_DPAD_DOWN));
}

static bool32 IsCancelEvent(const SDL_Event *event, enum PlatformInputAction closeAction)
{
    if (event->type == SDL_CONTROLLERBUTTONDOWN)
        return event->cbutton.button == SDL_CONTROLLER_BUTTON_B;
    return event->type == SDL_KEYDOWN && !event->key.repeat
        && (event->key.keysym.scancode == SDL_SCANCODE_ESCAPE
         || KeyMatchesAction(&event->key, closeAction));
}

static const char *SlotLabel(u8 slot, char *buffer, u32 size)
{
    if (slot == PLATFORM_STATE_QUICK_SLOT)
        return "Quick Save";
    snprintf(buffer, size, "Slot %u", slot);
    return buffer;
}

static void DrawConfirmation(const char *title, const char *slotLabel, const char *verb)
{
    char line[128];
    Platform_UiDrawPanel();
    Platform_UiDrawHeader(title);
    snprintf(line, sizeof(line), "%s %s?", verb, slotLabel);
    Platform_UiDrawText(250, 210, line, (SDL_Color){255, 255, 255, 255});
    Platform_UiDrawText(250, 245, "ENTER / A / Y CONFIRM",
                        (SDL_Color){255, 235, 130, 255});
    Platform_UiDrawText(250, 265, "ESCAPE / B / N CANCEL",
                        (SDL_Color){150, 180, 200, 255});
    Platform_UiPresent();
}

static enum ConfirmationResult Confirm(const char *title, u8 slot, const char *verb)
{
    SDL_Event event;
    char labelBuffer[32];
    const char *label = SlotLabel(slot, labelBuffer, sizeof(labelBuffer));
    for (;;)
    {
        DrawConfirmation(title, label, verb);
        if (!Platform_UiNextEvent(&event))
            return CONFIRMATION_QUIT;
        if (IsAcceptEvent(&event)
         || (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_Y))
            return CONFIRMATION_ACCEPT;
        if ((event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_N)
         || IsCancelEvent(&event, PLATFORM_INPUT_ACTION_OPEN_STATE_MANAGER)
         || IsCancelEvent(&event, PLATFORM_INPUT_ACTION_MANUAL_SAVE))
            return CONFIRMATION_CANCEL;
    }
}

static void ReloadEntries(struct PlatformStateEntry *entries, SDL_Texture **textures)
{
    u8 slot;
    for (slot = PLATFORM_STATE_QUICK_SLOT; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
    {
        if (textures[slot] != NULL)
            SDL_DestroyTexture(textures[slot]);
        textures[slot] = NULL;
        Platform_StateGetEntry(slot, &entries[slot]);
        if (entries[slot].thumbnailAvailable)
            textures[slot] = Platform_StateThumbnailLoad(slot);
    }
}

static void DestroyTextures(SDL_Texture **textures)
{
    u8 slot;
    for (slot = PLATFORM_STATE_QUICK_SLOT; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
    {
        SDL_DestroyTexture(textures[slot]);
        textures[slot] = NULL;
    }
}

static void DrawThumbnail(SDL_Texture *texture, int x, int y, bool32 occupied)
{
    SDL_Rect thumbnail = {x, y, 57, 38};
    if (texture != NULL)
    {
        SDL_RenderCopy(sdlRenderer, texture, NULL, &thumbnail);
        SDL_SetRenderDrawColor(sdlRenderer, 90, 145, 185, 255);
        SDL_RenderDrawRect(sdlRenderer, &thumbnail);
        return;
    }
    SDL_SetRenderDrawColor(sdlRenderer, 20, 31, 49, 255);
    SDL_RenderFillRect(sdlRenderer, &thumbnail);
    SDL_SetRenderDrawColor(sdlRenderer, 65, 91, 118, 255);
    SDL_RenderDrawRect(sdlRenderer, &thumbnail);
    Platform_UiDrawText(x + 8, y + 16, occupied ? "NO IMAGE" : "EMPTY",
                        (SDL_Color){110, 135, 155, 255});
}

static void DrawManager(const struct PlatformStateEntry *entries, SDL_Texture *const *textures,
                        u8 cursor, const char *message)
{
    u8 slot;
    Platform_UiDrawPanel();
    Platform_UiDrawHeader("SAVE STATES");
    for (slot = PLATFORM_STATE_QUICK_SLOT; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
    {
        int y = 74 + slot * 43;
        char labelBuffer[32];
        const char *label = SlotLabel(slot, labelBuffer, sizeof(labelBuffer));
        const struct PlatformStateEntry *entry = &entries[slot];
        SDL_Color primary = slot == cursor
            ? (SDL_Color){255, 235, 130, 255} : (SDL_Color){235, 235, 235, 255};
        if (slot == cursor)
        {
            SDL_Rect highlight = {35, y - 2, 890, 42};
            SDL_SetRenderDrawColor(sdlRenderer, 25, 55, 82, 255);
            SDL_RenderFillRect(sdlRenderer, &highlight);
        }
        DrawThumbnail(textures[slot], 48, y, entry->occupied);
        Platform_UiDrawText(122, y + 1, label, primary);
        if (!entry->occupied)
        {
            Platform_UiDrawText(122, y + 18, "Empty", (SDL_Color){125, 150, 170, 255});
        }
        else if (entry->profileConflict)
        {
            Platform_UiDrawText(122, y + 18, "Different profile - load blocked",
                                (SDL_Color){255, 140, 140, 255});
        }
        else if (!entry->metadataAvailable)
        {
            Platform_UiDrawText(122, y + 18, "Metadata unavailable",
                                (SDL_Color){155, 180, 195, 255});
        }
        else
        {
            char playTime[32];
            char timestamp[64];
            char details[128];
            Platform_StateMetadataFormatPlayTime(&entry->metadata, playTime, sizeof(playTime));
            Platform_StateMetadataFormatTimestamp(&entry->metadata, timestamp, sizeof(timestamp));
            Platform_UiDrawText(122, y + 14, entry->metadata.location,
                                (SDL_Color){180, 215, 230, 255});
            snprintf(details, sizeof(details), "%s   %s   %s",
                     playTime, timestamp, entry->metadata.context);
            Platform_UiDrawText(122, y + 27, details, (SDL_Color){135, 165, 185, 255});
        }
    }
    Platform_UiDrawText(42, 478, "UP/DOWN SELECT   ENTER/A LOAD   F7/S SAVE   DELETE REMOVE",
                        (SDL_Color){150, 180, 200, 255});
    Platform_UiDrawText(42, 495, message != NULL && message[0] != '\0'
                                     ? message : "ESCAPE/F6 CLOSE - F7 ON QUICK SAVE UPDATES IT",
                        message != NULL && message[0] != '\0'
                                     ? (SDL_Color){255, 200, 125, 255}
                                     : (SDL_Color){150, 180, 200, 255});
    Platform_UiPresent();
}

static enum PlatformStateOperationResult SaveSlot(u8 slot, bool32 occupied,
                                                   enum ConfirmationResult *confirmation)
{
    *confirmation = CONFIRMATION_ACCEPT;
    if (occupied && slot != PLATFORM_STATE_QUICK_SLOT)
    {
        *confirmation = Confirm("OVERWRITE STATE?", slot, "Overwrite");
        if (*confirmation != CONFIRMATION_ACCEPT)
            return PLATFORM_STATE_OPERATION_FAILED;
    }
    return Platform_StateSave(slot);
}

static void FinishUi(void)
{
    Platform_InputClearState();
    Platform_VideoEndHostUi();
}

enum PlatformStateUiResult Platform_StateUiRunManager(void)
{
    struct PlatformStateEntry entries[STATE_ENTRY_COUNT];
    SDL_Texture *textures[STATE_ENTRY_COUNT] = {0};
    SDL_Event event;
    u8 cursor = PLATFORM_STATE_QUICK_SLOT;
    char message[256] = "";
    enum PlatformStateUiResult uiResult = PLATFORM_STATE_UI_CLOSED;

    Platform_VideoBeginHostUi();
    ReloadEntries(entries, textures);
    for (;;)
    {
        DrawManager(entries, textures, cursor, message);
        if (!Platform_UiNextEvent(&event))
        {
            uiResult = PLATFORM_STATE_UI_QUIT;
            break;
        }
        if (IsUpEvent(&event))
        {
            cursor = cursor == PLATFORM_STATE_QUICK_SLOT ? PLATFORM_STATE_SLOT_COUNT : cursor - 1;
            message[0] = '\0';
        }
        else if (IsDownEvent(&event))
        {
            cursor = cursor == PLATFORM_STATE_SLOT_COUNT ? PLATFORM_STATE_QUICK_SLOT : cursor + 1;
            message[0] = '\0';
        }
        else if (IsCancelEvent(&event, PLATFORM_INPUT_ACTION_OPEN_STATE_MANAGER))
            break;
        else if (IsAcceptEvent(&event))
        {
            if (!entries[cursor].occupied)
                snprintf(message, sizeof(message), "Selected state is empty");
            else if (Platform_StateLoad(cursor) == PLATFORM_STATE_OPERATION_OK)
            {
                uiResult = PLATFORM_STATE_UI_LOADED;
                break;
            }
            else
                snprintf(message, sizeof(message), "%s", Platform_StateGetLastError());
        }
        else if (event.type == SDL_KEYDOWN && !event.key.repeat
              && (event.key.keysym.scancode == SDL_SCANCODE_S
               || KeyMatchesAction(&event.key, PLATFORM_INPUT_ACTION_MANUAL_SAVE)))
        {
            enum ConfirmationResult confirmation;
            enum PlatformStateOperationResult result = SaveSlot(cursor, entries[cursor].occupied,
                                                                 &confirmation);
            if (confirmation == CONFIRMATION_QUIT)
            {
                uiResult = PLATFORM_STATE_UI_QUIT;
                break;
            }
            if (confirmation == CONFIRMATION_CANCEL)
                message[0] = '\0';
            else if (result == PLATFORM_STATE_OPERATION_FAILED)
                snprintf(message, sizeof(message), "%s", Platform_StateGetLastError());
            else
            {
                snprintf(message, sizeof(message), "%s",
                         result == PLATFORM_STATE_OPERATION_OK ? "State saved" : Platform_StateGetLastError());
                ReloadEntries(entries, textures);
            }
        }
        else if (event.type == SDL_KEYDOWN && !event.key.repeat
              && event.key.keysym.scancode == SDL_SCANCODE_DELETE)
        {
            if (!entries[cursor].occupied)
                snprintf(message, sizeof(message), "Selected state is already empty");
            else
            {
                enum ConfirmationResult confirmation = Confirm("DELETE STATE?", cursor, "Delete");
                if (confirmation == CONFIRMATION_QUIT)
                {
                    uiResult = PLATFORM_STATE_UI_QUIT;
                    break;
                }
                if (confirmation == CONFIRMATION_ACCEPT)
                {
                    if (Platform_StateDelete(cursor) == PLATFORM_STATE_OPERATION_OK)
                    {
                        snprintf(message, sizeof(message), "State deleted");
                        ReloadEntries(entries, textures);
                    }
                    else
                        snprintf(message, sizeof(message), "%s", Platform_StateGetLastError());
                }
            }
        }
    }
    DestroyTextures(textures);
    FinishUi();
    return uiResult;
}

static void DrawSavePicker(const struct PlatformStateEntry *entries, u8 cursor, const char *message)
{
    u8 slot;
    Platform_UiDrawPanel();
    Platform_UiDrawHeader("SAVE TO SLOT");
    for (slot = 1; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
    {
        int y = 105 + (slot - 1) * 38;
        char label[32];
        SDL_Color color = slot == cursor
            ? (SDL_Color){255, 235, 130, 255} : (SDL_Color){235, 235, 235, 255};
        if (slot == cursor)
        {
            SDL_Rect highlight = {185, y - 10, 590, 29};
            SDL_SetRenderDrawColor(sdlRenderer, 25, 55, 82, 255);
            SDL_RenderFillRect(sdlRenderer, &highlight);
        }
        snprintf(label, sizeof(label), "%s SLOT %u", slot == cursor ? ">" : " ", slot);
        Platform_UiDrawText(210, y, label, color);
        Platform_UiDrawText(480, y, entries[slot].occupied ? "Occupied" : "Empty",
                            entries[slot].occupied ? (SDL_Color){180, 215, 230, 255}
                                                   : (SDL_Color){125, 150, 170, 255});
    }
    Platform_UiDrawText(205, 430, "UP/DOWN SELECT   ENTER/A SAVE",
                        (SDL_Color){150, 180, 200, 255});
    Platform_UiDrawText(205, 450, "ESCAPE/F7 CANCEL",
                        (SDL_Color){150, 180, 200, 255});
    if (message != NULL && message[0] != '\0')
        Platform_UiDrawText(205, 480, message, (SDL_Color){255, 160, 140, 255});
    Platform_UiPresent();
}

enum PlatformStateUiResult Platform_StateUiRunSavePicker(void)
{
    struct PlatformStateEntry entries[STATE_ENTRY_COUNT];
    SDL_Event event;
    u8 cursor = 1;
    u8 slot;
    char message[256] = "";
    enum PlatformStateUiResult uiResult = PLATFORM_STATE_UI_CLOSED;

    Platform_VideoBeginHostUi();
    for (slot = 1; slot <= PLATFORM_STATE_SLOT_COUNT; slot++)
        Platform_StateGetEntry(slot, &entries[slot]);
    for (;;)
    {
        DrawSavePicker(entries, cursor, message);
        if (!Platform_UiNextEvent(&event))
        {
            uiResult = PLATFORM_STATE_UI_QUIT;
            break;
        }
        if (IsUpEvent(&event))
        {
            cursor = cursor == 1 ? PLATFORM_STATE_SLOT_COUNT : cursor - 1;
            message[0] = '\0';
        }
        else if (IsDownEvent(&event))
        {
            cursor = cursor == PLATFORM_STATE_SLOT_COUNT ? 1 : cursor + 1;
            message[0] = '\0';
        }
        else if (IsCancelEvent(&event, PLATFORM_INPUT_ACTION_MANUAL_SAVE))
            break;
        else if (IsAcceptEvent(&event))
        {
            enum ConfirmationResult confirmation;
            enum PlatformStateOperationResult result = SaveSlot(cursor, entries[cursor].occupied,
                                                                 &confirmation);
            if (confirmation == CONFIRMATION_QUIT)
            {
                uiResult = PLATFORM_STATE_UI_QUIT;
                break;
            }
            if (confirmation == CONFIRMATION_CANCEL)
                message[0] = '\0';
            else if (result == PLATFORM_STATE_OPERATION_FAILED)
                snprintf(message, sizeof(message), "%s", Platform_StateGetLastError());
            else
            {
                uiResult = PLATFORM_STATE_UI_SAVED;
                break;
            }
        }
    }
    FinishUi();
    return uiResult;
}

#endif
