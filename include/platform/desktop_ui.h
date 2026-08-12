#ifndef GUARD_PLATFORM_DESKTOP_UI_H
#define GUARD_PLATFORM_DESKTOP_UI_H

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "gba/types.h"

void Platform_UiDrawText(int x, int y, const char *text, SDL_Color color);
void Platform_UiDrawPanel(void);
void Platform_UiDrawHeader(const char *title);
void Platform_UiPresent(void);
bool32 Platform_UiNextEvent(SDL_Event *event);

#endif
