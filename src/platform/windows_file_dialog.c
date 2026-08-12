#if defined(PLATFORM_SDL2) && defined(_WIN32)

#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include "global.h"
#include "platform/desktop_file_dialog.h"

bool32 Platform_FileDialogSelectEmeraldRom(char *path, u32 pathSize)
{
    wchar_t widePath[32768] = L"";
    OPENFILENAMEW dialog;
    int length;

    if (path == NULL || pathSize < 2)
        return FALSE;
    path[0] = '\0';
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = NULL;
    dialog.lpstrFile = widePath;
    dialog.nMaxFile = sizeof(widePath) / sizeof(widePath[0]);
    dialog.lpstrFilter = L"Game Boy Advance ROM (*.gba)\0*.gba\0All files\0*.*\0\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrTitle = L"Select Pokemon Emerald ROM";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY
                 | OFN_DONTADDTORECENT;
    if (!GetOpenFileNameW(&dialog))
        return FALSE;
    length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, widePath, -1,
                                 path, (int)pathSize, NULL, NULL);
    if (length <= 0)
    {
        path[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

#endif
