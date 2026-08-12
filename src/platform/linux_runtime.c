#if defined(PLATFORM_SDL2) && defined(__linux__)

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "global.h"
#include "platform/desktop_runtime.h"

extern unsigned char __bss_start[];
extern unsigned char __executable_start[];
extern unsigned char _end[];

bool32 Platform_RuntimeGetImageRange(uintptr_t *start, uintptr_t *end)
{
    if (start == NULL || end == NULL)
        return FALSE;
    *start = (uintptr_t)__executable_start;
    *end = (uintptr_t)_end;
    return *end > *start;
}

bool32 Platform_RuntimeGetBssRange(uintptr_t *start, uintptr_t *end)
{
    if (start == NULL || end == NULL)
        return FALSE;
    *start = (uintptr_t)__bss_start;
    *end = (uintptr_t)_end;
    return *end >= *start;
}

bool32 Platform_RuntimeAddressIsExecutable(uintptr_t address)
{
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[256];

    if (maps == NULL)
        return FALSE;
    while (fgets(line, sizeof(line), maps) != NULL)
    {
        unsigned long begin;
        unsigned long end;
        char permissions[5];

        if (sscanf(line, "%lx-%lx %4s", &begin, &end, permissions) == 3
         && address >= (uintptr_t)begin && address < (uintptr_t)end)
        {
            bool32 executable = permissions[2] == 'x';
            fclose(maps);
            return executable;
        }
    }
    fclose(maps);
    return FALSE;
}

bool32 Platform_RuntimeAddressIsMapped(uintptr_t address)
{
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t page;
    unsigned char resident;

    if (pageSize <= 0)
        return FALSE;
    page = address & ~((uintptr_t)pageSize - 1);
    return mincore((void *)page, (size_t)pageSize, &resident) == 0;
}

bool32 Platform_RuntimeGetBuildId(const char *abiId, char *dest, u32 destSize)
{
    if (abiId == NULL || dest == NULL || destSize == 0)
        return FALSE;
#if UINTPTR_MAX > UINT32_MAX
    {
        FILE *file = NULL;
        Elf64_Ehdr header;
        Elf64_Phdr *programs = NULL;
        bool32 found = FALSE;
        u32 i;

        file = fopen("/proc/self/exe", "rb");
        if (file == NULL || fread(&header, sizeof(header), 1, file) != 1
         || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
         || header.e_ident[EI_CLASS] != ELFCLASS64
         || (header.e_type != ET_EXEC && header.e_type != ET_DYN)
         || header.e_phentsize != sizeof(Elf64_Phdr) || header.e_phnum == 0)
            goto finish;
        programs = malloc((size_t)header.e_phnum * sizeof(*programs));
        if (programs == NULL
         || fseek(file, (long)header.e_phoff, SEEK_SET) != 0
         || fread(programs, sizeof(*programs), header.e_phnum, file) != header.e_phnum)
            goto finish;
        for (i = 0; i < header.e_phnum && !found; i++)
        {
            u8 *notes;
            size_t offset = 0;

            if (programs[i].p_type != PT_NOTE || programs[i].p_filesz > SIZE_MAX)
                continue;
            notes = malloc((size_t)programs[i].p_filesz);
            if (notes == NULL)
                goto finish;
            if (fseek(file, (long)programs[i].p_offset, SEEK_SET) != 0
             || fread(notes, 1, (size_t)programs[i].p_filesz, file) != programs[i].p_filesz)
            {
                free(notes);
                goto finish;
            }
            while (offset + sizeof(Elf64_Nhdr) <= programs[i].p_filesz)
            {
                Elf64_Nhdr note;
                size_t nameOffset;
                size_t descOffset;
                size_t nextOffset;
                size_t required;
                u32 byte;

                memcpy(&note, notes + offset, sizeof(note));
                nameOffset = offset + sizeof(note);
                descOffset = nameOffset + ((note.n_namesz + 3u) & ~3u);
                nextOffset = descOffset + ((note.n_descsz + 3u) & ~3u);
                if (descOffset < nameOffset || nextOffset < descOffset
                 || nextOffset > programs[i].p_filesz)
                    break;
                required = strlen(abiId) + 1u + 2u * note.n_descsz + 1u;
                if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz >= 3
                 && memcmp(notes + nameOffset, "GNU", 3) == 0
                 && required <= destSize)
                {
                    int written = snprintf(dest, destSize, "%s-", abiId);

                    if (written < 0 || (u32)written >= destSize)
                        break;
                    for (byte = 0; byte < note.n_descsz; byte++)
                        snprintf(dest + written + byte * 2,
                                 destSize - (u32)written - byte * 2,
                                 "%02x", notes[descOffset + byte]);
                    found = TRUE;
                    break;
                }
                offset = nextOffset;
            }
            free(notes);
        }

finish:
        free(programs);
        if (file != NULL)
            fclose(file);
        return found;
    }
#else
    return snprintf(dest, destSize, "%s", abiId) > 0;
#endif
}

#endif
