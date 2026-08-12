#if defined(PLATFORM_SDL2) && !defined(_WIN32)

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "global.h"
#include "platform/desktop_file_dialog.h"

enum FilePickerResult
{
    FILE_PICKER_SELECTED,
    FILE_PICKER_CANCELLED,
    FILE_PICKER_FAILED,
};

struct FilePicker
{
    const char *name;
    const char *command;
    int cancelStatus;
};

static const struct FilePicker sFilePickers[] =
{
    {
        "zenity",
        "zenity --file-selection --title='Select Pokemon Emerald ROM' "
        "--file-filter='Game Boy Advance ROM (*.gba) | *.gba' "
        "--file-filter='All files | *'",
        1,
    },
    {
        "kdialog",
        "kdialog --title='Select Pokemon Emerald ROM' "
        "--getopenfilename . 'Game Boy Advance ROM (*.gba)'",
        1,
    },
    {
        "yad",
        "yad --file --title='Select Pokemon Emerald ROM' "
        "--file-filter='Game Boy Advance ROM (*.gba) | *.gba' "
        "--file-filter='All files | *'",
        1,
    },
};

static bool32 IsCommandAvailable(const char *name)
{
    char command[64];
    FILE *probe;
    int length;
    int status;

    length = snprintf(command, sizeof(command), "command -v '%s'", name);
    if (length < 0 || (size_t)length >= sizeof(command))
        return FALSE;

    probe = popen(command, "r");
    if (probe == NULL)
    {
        fprintf(stderr, "[file dialog] Could not check for %s: %s\n", name, strerror(errno));
        return FALSE;
    }

    status = pclose(probe);
    if (status == -1)
    {
        fprintf(stderr, "[file dialog] Could not check for %s: %s\n", name, strerror(errno));
        return FALSE;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool32 PickerWasCancelled(const struct FilePicker *picker, int status)
{
    return status != -1
        && WIFEXITED(status)
        && WEXITSTATUS(status) == picker->cancelStatus;
}

static void LogPickerFailure(const struct FilePicker *picker, int status)
{
    if (status == -1)
    {
        fprintf(stderr, "[file dialog] %s failed while waiting: %s\n",
                picker->name, strerror(errno));
    }
    else if (WIFEXITED(status))
    {
        fprintf(stderr, "[file dialog] %s failed with exit status %d\n",
                picker->name, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        fprintf(stderr, "[file dialog] %s was terminated by signal %d\n",
                picker->name, WTERMSIG(status));
    }
    else
    {
        fprintf(stderr, "[file dialog] %s failed (status 0x%x)\n",
                picker->name, status);
    }
}

static enum FilePickerResult RunFilePicker(const struct FilePicker *picker,
                                           char *path, u32 pathSize)
{
    FILE *dialog;
    int readSize;
    int status;
    int character;
    bool32 gotPath;
    bool32 pathWasTruncated;
    size_t length;

    path[0] = '\0';
    readSize = pathSize > INT_MAX ? INT_MAX : (int)pathSize;
    dialog = popen(picker->command, "r");
    if (dialog == NULL)
    {
        fprintf(stderr, "[file dialog] Could not launch %s: %s\n",
                picker->name, strerror(errno));
        return FILE_PICKER_FAILED;
    }

    gotPath = fgets(path, readSize, dialog) != NULL;
    pathWasTruncated = FALSE;
    if (gotPath)
    {
        length = strlen(path);
        pathWasTruncated = length == (size_t)readSize - 1
                        && strchr(path, '\n') == NULL;

        /* Drain command output before pclose if a broken picker writes more
         * than the path buffer can hold. */
        while ((character = fgetc(dialog)) != EOF)
        {
            if (character == '\n')
                break;
        }
    }

    status = pclose(dialog);
    if (!gotPath || pathWasTruncated)
    {
        path[0] = '\0';
        if (PickerWasCancelled(picker, status))
            return FILE_PICKER_CANCELLED;
        if (pathWasTruncated)
            fprintf(stderr, "[file dialog] %s returned a path that is too long\n", picker->name);
        else
            LogPickerFailure(picker, status);
        return FILE_PICKER_FAILED;
    }

    length = strlen(path);
    while (length != 0 && (path[length - 1] == '\n' || path[length - 1] == '\r'))
        path[--length] = '\0';

    if (status == 0 && length != 0)
        return FILE_PICKER_SELECTED;

    path[0] = '\0';
    if (PickerWasCancelled(picker, status))
        return FILE_PICKER_CANCELLED;
    if (status == 0)
        fprintf(stderr, "[file dialog] %s completed without returning a path\n", picker->name);
    else
        LogPickerFailure(picker, status);
    return FILE_PICKER_FAILED;
}

enum PlatformFileDialogResult Platform_FileDialogSelectEmeraldRom(char *path, u32 pathSize)
{
    size_t i;
    bool32 foundPicker;

    if (path == NULL || pathSize < 2)
        return PLATFORM_FILE_DIALOG_FAILED;
    path[0] = '\0';
    foundPicker = FALSE;

    for (i = 0; i < sizeof(sFilePickers) / sizeof(sFilePickers[0]); i++)
    {
        enum FilePickerResult result;

        if (!IsCommandAvailable(sFilePickers[i].name))
            continue;
        foundPicker = TRUE;
        result = RunFilePicker(&sFilePickers[i], path, pathSize);
        if (result == FILE_PICKER_SELECTED)
            return PLATFORM_FILE_DIALOG_SELECTED;
        if (result == FILE_PICKER_CANCELLED)
            return PLATFORM_FILE_DIALOG_CANCELLED;
    }

    path[0] = '\0';
    if (!foundPicker)
    {
        fprintf(stderr, "[file dialog] No supported file picker was found. "
                        "Install one of: zenity, kdialog, yad.\n");
        return PLATFORM_FILE_DIALOG_NO_PICKER_AVAILABLE;
    }

    fprintf(stderr, "[file dialog] All available supported file pickers failed "
                    "(tried in order: zenity, kdialog, yad).\n");
    return PLATFORM_FILE_DIALOG_FAILED;
}

#endif
