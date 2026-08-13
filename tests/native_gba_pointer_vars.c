#include "platform/host_memory.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    u16 target = 0x1234;
    GbaAddr address = HostPointerToGbaAddr(&target);
    s16 lo = address & 0xffff;
    s16 hi = address >> 16;
    GbaAddr rebuilt = (u16)lo | ((u16)hi << 16);

    // This is the two-s16 boundary used by StorePointerInVars. In a native
    // 64-bit process the value must be a registered GBA-address handle, not a
    // truncated copy of the host pointer.
    assert(rebuilt == address);
    assert(HostAddressIsRegisteredRuntimeHandle(address));
    assert(HostResolveGbaAddr(rebuilt) == &target);
    printf("native GBA pointer pair resolved: 0x%08x\n", rebuilt);
    return 0;
}
