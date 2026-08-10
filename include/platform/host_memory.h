#ifndef GUARD_PLATFORM_HOST_MEMORY_H
#define GUARD_PLATFORM_HOST_MEMORY_H

#include <stddef.h>
#include "gba/types.h"

// A host pointer which must cross a GBA-shaped 32-bit field is represented by
// a short-lived handle on 64-bit hosts. Generated fixed-width addresses are
// resolved through the same boundary.
GbaAddr HostPointerToGbaAddr(const void *ptr);
void *HostResolveGbaAddr(GbaAddr addr);
GbaAddr HostFunctionToGbaAddr(const void *functionPointerBytes, size_t size);
void HostResolveFunction(GbaAddr addr, void *functionPointerBytes, size_t size);
void HostAssertMemoryRange(const void *ptr, size_t size, const char *owner);

// GNU C's typeof keeps this helper usable for the several callback signatures
// that are packed into vanilla two-halfword task fields.
#define HOST_FUNCTION_ADDR(function) \
    ({ __typeof__(&(function)) hostFunction = &(function); \
       HostFunctionToGbaAddr(&hostFunction, sizeof(hostFunction)); })

#endif // GUARD_PLATFORM_HOST_MEMORY_H
