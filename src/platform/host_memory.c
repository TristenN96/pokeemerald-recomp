#include "global.h"
#include "platform/host_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void abort(void);

#define HOST_HANDLE_BASE 0xF0000000u
#define HOST_FUNCTION_HANDLE_BASE 0xE0000000u
#define HOST_HANDLE_INDEX_MASK 0x0000FFFFu
#define HOST_HANDLE_CAPACITY (HOST_HANDLE_INDEX_MASK + 1)

static const void *sHostPointers[HOST_HANDLE_CAPACITY];
static unsigned char sHostFunctions[HOST_HANDLE_CAPACITY][sizeof(uintptr_t)];
static u32 sNextHostHandle = 1;
static u32 sNextHostFunctionHandle = 1;

static void HostMemoryAbort(const char *message, uintptr_t addr)
{
    fprintf(stderr, "host memory error: %s (address=0x%zx)\n", message, addr);
    abort();
}

GbaAddr HostPointerToGbaAddr(const void *ptr)
{
    uintptr_t native;
    u32 i;

    if (ptr == NULL)
        return 0;

#if UINTPTR_MAX <= UINT32_MAX
    return (GbaAddr)(uintptr_t)ptr;
#else
    native = (uintptr_t)ptr;

    // Non-PIE executable and generated-data addresses are intentionally kept
    // in the logical 32-bit GBA address space. This is also required for
    // pointer-bearing state that can be copied into a save block and loaded
    // by a later process. Only transient host allocations need handles.
    if (native <= UINT32_MAX)
        return (GbaAddr)native;

    for (i = 1; i < sNextHostHandle; i++)
    {
        if (sHostPointers[i] == ptr)
            return HOST_HANDLE_BASE | i;
    }

    if (sNextHostHandle >= HOST_HANDLE_CAPACITY)
        HostMemoryAbort("host pointer handle table exhausted", native);

    sHostPointers[sNextHostHandle] = ptr;
    return HOST_HANDLE_BASE | sNextHostHandle++;
#endif
}

void *HostResolveGbaAddr(GbaAddr addr)
{
    u32 index;

    if (addr == 0)
        return NULL;

#if UINTPTR_MAX > UINT32_MAX
    if ((addr & 0xFFFF0000u) == HOST_HANDLE_BASE)
    {
        index = addr & HOST_HANDLE_INDEX_MASK;
        if (index == 0 || index >= sNextHostHandle || sHostPointers[index] == NULL)
            HostMemoryAbort("invalid host pointer handle", addr);
        return (void *)sHostPointers[index];
    }
#endif

    // Non-handle values are generated four-byte symbolic references. The
    // linux64 link is non-PIE and keeps vanilla generated data below 4 GiB;
    // retaining this explicit conversion makes the contract visible and
    // allows a future relocation table to replace it without changing users.
    return (void *)(uintptr_t)addr;
}

GbaAddr HostFunctionToGbaAddr(const void *functionPointerBytes, size_t size)
{
    uintptr_t native = 0;
    u32 i;

    if (functionPointerBytes == NULL || size == 0 || size > sizeof(native))
        HostMemoryAbort("invalid function pointer representation", 0);

    memcpy(&native, functionPointerBytes, size);

#if UINTPTR_MAX <= UINT32_MAX
    return (GbaAddr)native;
#else
    if (native <= UINT32_MAX)
        return (GbaAddr)native;

    for (i = 1; i < sNextHostFunctionHandle; i++)
    {
        if (memcmp(sHostFunctions[i], functionPointerBytes, size) == 0)
            return HOST_FUNCTION_HANDLE_BASE | i;
    }

    if (sNextHostFunctionHandle >= HOST_HANDLE_CAPACITY)
        HostMemoryAbort("host function handle table exhausted", 0);

    memcpy(sHostFunctions[sNextHostFunctionHandle], functionPointerBytes, size);
    return HOST_FUNCTION_HANDLE_BASE | sNextHostFunctionHandle++;
#endif
}

void HostResolveFunction(GbaAddr addr, void *functionPointerBytes, size_t size)
{
    uintptr_t native;
    u32 index;

    if (functionPointerBytes == NULL || size == 0 || size > sizeof(native))
        HostMemoryAbort("invalid function pointer destination", addr);

#if UINTPTR_MAX > UINT32_MAX
    if ((addr & 0xFFFF0000u) == HOST_FUNCTION_HANDLE_BASE)
    {
        index = addr & HOST_HANDLE_INDEX_MASK;
        if (index == 0 || index >= sNextHostFunctionHandle)
            HostMemoryAbort("invalid host function handle", addr);
        memcpy(functionPointerBytes, sHostFunctions[index], size);
        return;
    }
#endif

    native = addr;
    memset(functionPointerBytes, 0, size);
    memcpy(functionPointerBytes, &native, size);
}

void HostAssertMemoryRange(const void *ptr, size_t size, const char *owner)
{
    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end;
    uintptr_t regionStart;
    uintptr_t regionEnd;

    if (ptr == NULL && size != 0)
    {
        fprintf(stderr, "host memory error: %s received NULL for %zu bytes\n", owner, size);
        abort();
    }

    if (size > UINTPTR_MAX - start)
    {
        fprintf(stderr, "host memory error: %s range overflow\n", owner);
        abort();
    }
    end = start + size;

#define CHECK_REGION(region, regionSize) \
    do { \
        regionStart = (uintptr_t)(region); \
        regionEnd = regionStart + (regionSize); \
        if (start >= regionStart && start <= regionEnd && end > regionEnd) \
        { \
            fprintf(stderr, "host memory error: %s exceeds %s (%zu bytes)\n", owner, #region, (size_t)(regionSize)); \
            abort(); \
        } \
    } while (0)

    if (size != 0)
    {
        CHECK_REGION(REG_BASE, 0x400);
        CHECK_REGION(VRAM_, VRAM_SIZE);
        CHECK_REGION(PLTT, PLTT_SIZE);
        CHECK_REGION(OAM, OAM_SIZE);
    }

#undef CHECK_REGION
}
