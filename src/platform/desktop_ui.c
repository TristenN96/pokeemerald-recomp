#ifdef PLATFORM_SDL2

#include <ctype.h>

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "global.h"
#include "platform/desktop_ui.h"
#include "platform/desktop_video.h"

static const u8 sLetters[26][7] =
{
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
};

static const u8 sDigits[10][7] =
{
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
};

static u8 GlyphRow(char character, int row)
{
    if (character >= 'a' && character <= 'z') character = (char)toupper((unsigned char)character);
    if (character >= 'A' && character <= 'Z') return sLetters[character - 'A'][row];
    if (character >= '0' && character <= '9') return sDigits[character - '0'][row];
    switch (character)
    {
    case ' ': return 0;
    case '-': return row == 3 ? 14 : 0;
    case '+': return row == 3 ? 14 : (row == 1 || row == 2 || row == 4 || row == 5 ? 4 : 0);
    case ':': return (row == 2 || row == 5) ? 4 : 0;
    case '.': return row == 6 ? 4 : 0;
    case ',': return row == 6 ? 8 : row == 5 ? 4 : 0;
    case '\'': return row == 0 || row == 1 ? 4 : 0;
    case '/': return row == 0 || row == 6 ? 1 : (1 << (6 - row));
    case '_': return row == 6 ? 31 : 0;
    case '>': return row == 3 ? 16 : (row == 2 || row == 4 ? 8 : (row == 1 || row == 5 ? 4 : 0));
    case '?': return row == 0 ? 14 : row == 1 ? 17 : row == 2 ? 1 : row == 3 ? 2 : row == 6 ? 4 : 0;
    case '!': return row < 5 ? 4 : row == 6 ? 4 : 0;
    case '[': return row == 0 || row == 6 ? 31 : 16;
    case ']': return row == 0 || row == 6 ? 31 : 1;
    default: return 0;
    }
}

void Platform_UiDrawText(int x, int y, const char *text, SDL_Color color)
{
    int i;
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    for (i = 0; text != NULL && text[i] != '\0'; i++)
    {
        int row;
        for (row = 0; row < 7; row++)
        {
            int column;
            u8 bits = GlyphRow(text[i], row);
            for (column = 0; column < 5; column++)
            {
                if (bits & (1 << (4 - column)))
                {
                    SDL_Rect pixel = {x + i * 6 + column, y + row, 1, 1};
                    SDL_RenderFillRect(sdlRenderer, &pixel);
                }
            }
        }
    }
}

void Platform_UiDrawPanel(void)
{
    SDL_Rect border = {14, 14, 932, 512};
    SDL_SetRenderDrawColor(sdlRenderer, 9, 18, 35, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_SetRenderDrawColor(sdlRenderer, 36, 76, 116, 255);
    SDL_RenderDrawRect(sdlRenderer, &border);
}

void Platform_UiDrawHeader(const char *title)
{
    Platform_UiDrawText(34, 30, "POKEMON EMERALD - PC QUALITY OF LIFE",
                        (SDL_Color){120, 210, 255, 255});
    Platform_UiDrawText(34, 52, title, (SDL_Color){255, 235, 130, 255});
}

void Platform_UiPresent(void)
{
    SDL_RenderPresent(sdlRenderer);
}

bool32 Platform_UiNextEvent(SDL_Event *event)
{
    while (!SDL_PollEvent(event))
        SDL_Delay(8);
    return event->type != SDL_QUIT;
}

#endif
