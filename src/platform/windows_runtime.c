#if defined(PLATFORM_SDL2) && defined(_WIN32)

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "global.h"
#include "platform/desktop_runtime.h"

extern unsigned char __bss_start__[];
extern unsigned char __bss_end__[];
extern unsigned char __start_game_bss[];
extern unsigned char __stop_game_bss[];
extern unsigned char __start_game_data[];
extern unsigned char __stop_game_data[];

static bool32 GetModuleHeaders(uintptr_t *base, IMAGE_NT_HEADERS64 **headers)
{
    HMODULE module = GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS64 *nt;

    if (module == NULL)
        return FALSE;
    dos = (IMAGE_DOS_HEADER *)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return FALSE;
    nt = (IMAGE_NT_HEADERS64 *)((u8 *)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE
     || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return FALSE;
    if (base != NULL)
        *base = (uintptr_t)module;
    if (headers != NULL)
        *headers = nt;
    return TRUE;
}

bool32 Platform_RuntimeGetImageRange(uintptr_t *start, uintptr_t *end)
{
    uintptr_t base;
    IMAGE_NT_HEADERS64 *headers;

    if (start == NULL || end == NULL || !GetModuleHeaders(&base, &headers))
        return FALSE;
    if (headers->OptionalHeader.SizeOfImage > UINTPTR_MAX - base)
        return FALSE;
    *start = base;
    *end = base + headers->OptionalHeader.SizeOfImage;
    return *end > *start;
}

bool32 Platform_RuntimeGetBssRange(uintptr_t *start, uintptr_t *end)
{
    if (start == NULL || end == NULL)
        return FALSE;
    *start = (uintptr_t)__bss_start__;
    *end = (uintptr_t)__bss_end__;
    return *end >= *start;
}

bool32 Platform_RuntimeGetGameBssRange(uintptr_t *start, uintptr_t *end)
{
    if (start == NULL || end == NULL)
        return FALSE;
    *start = (uintptr_t)__start_game_bss;
    *end = (uintptr_t)__stop_game_bss;
    return *end >= *start;
}

bool32 Platform_RuntimeGetGameDataRange(uintptr_t *start, uintptr_t *end)
{
    if (start == NULL || end == NULL)
        return FALSE;
    *start = (uintptr_t)__start_game_data;
    *end = (uintptr_t)__stop_game_data;
    return *end >= *start;
}

bool32 Platform_RuntimeAddressIsExecutable(uintptr_t address)
{
    uintptr_t base;
    IMAGE_NT_HEADERS64 *headers;
    IMAGE_SECTION_HEADER *section;
    u16 i;

    if (!GetModuleHeaders(&base, &headers))
        return FALSE;
    section = IMAGE_FIRST_SECTION(headers);
    for (i = 0; i < headers->FileHeader.NumberOfSections; i++, section++)
    {
        uintptr_t start = base + section->VirtualAddress;
        uintptr_t size = section->Misc.VirtualSize > section->SizeOfRawData
                       ? section->Misc.VirtualSize : section->SizeOfRawData;
        if (address >= start && address - start < size)
            return (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    }
    return FALSE;
}

bool32 Platform_RuntimeAddressIsMapped(uintptr_t address)
{
    MEMORY_BASIC_INFORMATION information;

    if (VirtualQuery((const void *)address, &information, sizeof(information)) == 0)
        return FALSE;
    return information.State == MEM_COMMIT
        && (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
}

bool32 Platform_RuntimeGetBuildId(const char *abiId, char *dest, u32 destSize)
{
    wchar_t path[32768];
    DWORD length;
    FILE *file;
    u64 hash = UINT64_C(1469598103934665603);
    u8 buffer[16384];
    size_t bytes;

    if (abiId == NULL || dest == NULL || destSize == 0)
        return FALSE;
    length = GetModuleFileNameW(NULL, path, ARRAY_COUNT(path));
    if (length == 0 || length >= ARRAY_COUNT(path))
        return FALSE;
    file = _wfopen(path, L"rb");
    if (file == NULL)
        return FALSE;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0)
    {
        size_t i;
        for (i = 0; i < bytes; i++)
        {
            hash ^= buffer[i];
            hash *= UINT64_C(1099511628211);
        }
    }
    if (ferror(file) || fclose(file) != 0)
        return FALSE;
    return snprintf(dest, destSize, "%s-%016llx", abiId,
                    (unsigned long long)hash) > 0;
}

#endif
