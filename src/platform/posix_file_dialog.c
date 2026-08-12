#if defined(PLATFORM_SDL2) && !defined(_WIN32)

#include <stdio.h>
#include <string.h>

#include "global.h"
#include "platform/desktop_file_dialog.h"

bool32 Platform_FileDialogSelectEmeraldRom(char *path, u32 pathSize)
{
    FILE *dialog;
    size_t length;
    int status;

    if (path == NULL || pathSize < 2)
        return FALSE;
    path[0] = '\0';
    dialog = popen("zenity --file-selection --title='Select Pokemon Emerald ROM' "
                   "--file-filter='Game Boy Advance ROM (*.gba) | *.gba' "
                   "--file-filter='All files | *' 2>/dev/null", "r");
    if (dialog == NULL)
        return FALSE;
    if (fgets(path, (int)pathSize, dialog) == NULL)
    {
        pclose(dialog);
        path[0] = '\0';
        return FALSE;
    }
    status = pclose(dialog);
    length = strlen(path);
    while (length != 0 && (path[length - 1] == '\n' || path[length - 1] == '\r'))
        path[--length] = '\0';
    return status == 0 && length != 0;
}

#endif
