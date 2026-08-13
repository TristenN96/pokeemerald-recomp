#include "global.h"
#include "task.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    assert(sizeof(TaskFunc) == sizeof(uintptr_t));
    assert(offsetof(struct Task, func) == 0x00);
    assert(offsetof(struct Task, isActive) == 0x08);
    assert(offsetof(struct Task, prev) == 0x09);
    assert(offsetof(struct Task, next) == 0x0A);
    assert(offsetof(struct Task, priority) == 0x0B);
    assert(offsetof(struct Task, data) == 0x0C);
    assert(sizeof(struct Task) == 0x30);
    return 0;
}
