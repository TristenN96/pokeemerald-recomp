#include "global.h"
#include "task.h"
#include "platform/host_memory.h"
#include <stddef.h>

extern void abort(void);

COMMON_DATA struct Task gTasks[NUM_TASKS] = {0};

#if defined(LINUX64) && LINUX64
STATIC_ASSERT(sizeof(TaskFunc) == sizeof(uintptr_t), NativeTaskFunctionPointerSize);
STATIC_ASSERT(offsetof(struct Task, func) == 0x00, NativeTaskFuncOffset);
STATIC_ASSERT(offsetof(struct Task, isActive) == 0x08, NativeTaskActiveOffset);
STATIC_ASSERT(offsetof(struct Task, prev) == 0x09, NativeTaskPrevOffset);
STATIC_ASSERT(offsetof(struct Task, next) == 0x0A, NativeTaskNextOffset);
STATIC_ASSERT(offsetof(struct Task, priority) == 0x0B, NativeTaskPriorityOffset);
STATIC_ASSERT(offsetof(struct Task, data) == 0x0C, NativeTaskDataOffset);
STATIC_ASSERT(sizeof(struct Task) == 0x30, NativeTaskSize);
#endif

#if defined(LINUX64) && LINUX64
// Follow-up callbacks are game-visible task data on the GBA, but are native
// function pointers on the host. Keep the 16-bit task fields untouched and
// retain the callback in a sidecar indexed by the stable task id.
HOST_DATA static TaskFunc sTaskFollowupFuncs[NUM_TASKS];
HOST_DATA static unsigned char sTaskStoredFunctions[NUM_TASKS][sizeof(uintptr_t)];
HOST_DATA static u32 sTaskStateFailureOffset;
HOST_DATA static uintptr_t sTaskStateFailureAddress;
#endif

static void InsertTask(u8 newTaskId);
static u8 FindFirstActiveTask(void);

void ResetTasks(void)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
    {
#if defined(LINUX64) && LINUX64
        sTaskFollowupFuncs[i] = NULL;
        memset(sTaskStoredFunctions[i], 0, sizeof(sTaskStoredFunctions[i]));
#endif
        gTasks[i].isActive = FALSE;
        gTasks[i].func = TaskDummy;
        gTasks[i].prev = i;
        gTasks[i].next = i + 1;
        gTasks[i].priority = -1;
        memset(gTasks[i].data, 0, sizeof(gTasks[i].data));
    }

    gTasks[0].prev = HEAD_SENTINEL;
    gTasks[NUM_TASKS - 1].next = TAIL_SENTINEL;
}

u8 CreateTask(TaskFunc func, u8 priority)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
    {
        if (!gTasks[i].isActive)
        {
#if defined(LINUX64) && LINUX64
            sTaskFollowupFuncs[i] = NULL;
            memset(sTaskStoredFunctions[i], 0, sizeof(sTaskStoredFunctions[i]));
#endif
            gTasks[i].func = func;
            gTasks[i].priority = priority;
            InsertTask(i);
            memset(gTasks[i].data, 0, sizeof(gTasks[i].data));
            gTasks[i].isActive = TRUE;
            return i;
        }
    }

    return 0;
}

static void InsertTask(u8 newTaskId)
{
    u8 taskId = FindFirstActiveTask();

    if (taskId == NUM_TASKS)
    {
        // The new task is the only task.
        gTasks[newTaskId].prev = HEAD_SENTINEL;
        gTasks[newTaskId].next = TAIL_SENTINEL;
        return;
    }

    while (1)
    {
        if (gTasks[newTaskId].priority < gTasks[taskId].priority)
        {
            // We've found a task with a higher priority value,
            // so we insert the new task before it.
            gTasks[newTaskId].prev = gTasks[taskId].prev;
            gTasks[newTaskId].next = taskId;
            if (gTasks[taskId].prev != HEAD_SENTINEL)
                gTasks[gTasks[taskId].prev].next = newTaskId;
            gTasks[taskId].prev = newTaskId;
            return;
        }
        if (gTasks[taskId].next == TAIL_SENTINEL)
        {
            // We've reached the end.
            gTasks[newTaskId].prev = taskId;
            gTasks[newTaskId].next = gTasks[taskId].next;
            gTasks[taskId].next = newTaskId;
            return;
        }
        taskId = gTasks[taskId].next;
    }
}

void DestroyTask(u8 taskId)
{
    if (gTasks[taskId].isActive)
    {
#if defined(LINUX64) && LINUX64
        sTaskFollowupFuncs[taskId] = NULL;
        memset(sTaskStoredFunctions[taskId], 0, sizeof(sTaskStoredFunctions[taskId]));
#endif
        gTasks[taskId].isActive = FALSE;

        if (gTasks[taskId].prev == HEAD_SENTINEL)
        {
            if (gTasks[taskId].next != TAIL_SENTINEL)
                gTasks[gTasks[taskId].next].prev = HEAD_SENTINEL;
        }
        else
        {
            if (gTasks[taskId].next == TAIL_SENTINEL)
            {
                gTasks[gTasks[taskId].prev].next = TAIL_SENTINEL;
            }
            else
            {
                gTasks[gTasks[taskId].prev].next = gTasks[taskId].next;
                gTasks[gTasks[taskId].next].prev = gTasks[taskId].prev;
            }
        }
    }
}

void RunTasks(void)
{
    u8 taskId = FindFirstActiveTask();

    if (taskId != NUM_TASKS)
    {
        do
        {
            gTasks[taskId].func(taskId);
            taskId = gTasks[taskId].next;
        } while (taskId != TAIL_SENTINEL);
    }
}

static u8 FindFirstActiveTask(void)
{
    u8 taskId;

    for (taskId = 0; taskId < NUM_TASKS; taskId++)
        if (gTasks[taskId].isActive == TRUE && gTasks[taskId].prev == HEAD_SENTINEL)
            break;

    return taskId;
}

void TaskDummy(u8 taskId)
{
}

void SetTaskFuncWithFollowupFunc(u8 taskId, TaskFunc func, TaskFunc followupFunc)
{
#if !defined(LINUX64) || !LINUX64
    u8 followupFuncIndex = NUM_TASK_DATA - 2; // Should be const.
#endif

#if defined(LINUX64) && LINUX64
    sTaskFollowupFuncs[taskId] = followupFunc;
#else
    gTasks[taskId].data[followupFuncIndex] = (s16)((u32)followupFunc);
    gTasks[taskId].data[followupFuncIndex + 1] = (s16)((u32)followupFunc >> 16); // Store followupFunc as two half-words in the data array.
#endif
    gTasks[taskId].func = func;
}

void SwitchTaskToFollowupFunc(u8 taskId)
{
#if !defined(LINUX64) || !LINUX64
    u8 followupFuncIndex = NUM_TASK_DATA - 2; // Should be const.
#endif

#if defined(LINUX64) && LINUX64
    gTasks[taskId].func = sTaskFollowupFuncs[taskId];
#else
    gTasks[taskId].func = (TaskFunc)((u16)(gTasks[taskId].data[followupFuncIndex]) | (gTasks[taskId].data[followupFuncIndex + 1] << 16));
#endif
}

#if defined(LINUX64) && LINUX64
void Task_StoreFunction(u8 taskId, const void *functionPointerBytes, u32 size)
{
    if (taskId >= NUM_TASKS || functionPointerBytes == NULL
     || size == 0 || size > sizeof(sTaskStoredFunctions[taskId]))
        abort();
    memset(sTaskStoredFunctions[taskId], 0, sizeof(sTaskStoredFunctions[taskId]));
    memcpy(sTaskStoredFunctions[taskId], functionPointerBytes, size);
}

void Task_LoadStoredFunction(u8 taskId, void *functionPointerBytes, u32 size)
{
    if (taskId >= NUM_TASKS || functionPointerBytes == NULL
     || size == 0 || size > sizeof(sTaskStoredFunctions[taskId]))
        abort();
    memcpy(functionPointerBytes, sTaskStoredFunctions[taskId], size);
}
#endif

bool8 FuncIsActiveTask(TaskFunc func)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
        if (gTasks[i].isActive == TRUE && gTasks[i].func == func)
            return TRUE;

    return FALSE;
}

u8 FindTaskIdByFunc(TaskFunc func)
{
    s32 i;

    for (i = 0; i < NUM_TASKS; i++)
        if (gTasks[i].isActive == TRUE && gTasks[i].func == func)
            return (u8)i;

    return TASK_NONE; // No task was found.
}

u8 GetTaskCount(void)
{
    u8 i;
    u8 count = 0;

    for (i = 0; i < NUM_TASKS; i++)
        if (gTasks[i].isActive == TRUE)
            count++;

    return count;
}

void SetWordTaskArg(u8 taskId, u8 dataElem, u32 value)
{
    if (dataElem < NUM_TASK_DATA - 1)
    {
        gTasks[taskId].data[dataElem] = value;
        gTasks[taskId].data[dataElem + 1] = value >> 16;
    }
}

u32 GetWordTaskArg(u8 taskId, u8 dataElem)
{
    if (dataElem < NUM_TASK_DATA - 1)
        return (u16)gTasks[taskId].data[dataElem] | (gTasks[taskId].data[dataElem + 1] << 16);
    else
        return 0;
}

u32 Task_GetStateSize(void)
{
#if defined(LINUX64) && LINUX64
    return NUM_TASKS * TASK_STATE_FUNCTIONS_PER_TASK
         * sizeof(struct HostPersistentAddress);
#else
    return 0;
#endif
}

bool32 Task_SaveState(void *dest, u32 size)
{
#if defined(LINUX64) && LINUX64
    u32 i;
    if (dest == NULL || size != Task_GetStateSize())
        return FALSE;
    sTaskStateFailureOffset = 0;
    sTaskStateFailureAddress = 0;
    for (i = 0; i < NUM_TASKS; i++)
    {
        uintptr_t native = 0;
        struct HostPersistentAddress callbacks[TASK_STATE_FUNCTIONS_PER_TASK];
        u32 record = i * TASK_STATE_FUNCTIONS_PER_TASK;

        memcpy(&native, &sTaskFollowupFuncs[i], sizeof(native));
        if (!HostFunctionToPersistentAddress(&sTaskFollowupFuncs[i],
                                             sizeof(sTaskFollowupFuncs[i]), &callbacks[0]))
        {
            sTaskStateFailureOffset = record * sizeof(callbacks[0]);
            sTaskStateFailureAddress = native;
            return FALSE;
        }
        native = 0;
        memcpy(&native, sTaskStoredFunctions[i], sizeof(native));
        if (!HostFunctionToPersistentAddress(sTaskStoredFunctions[i],
                                             sizeof(sTaskStoredFunctions[i]), &callbacks[1]))
        {
            sTaskStateFailureOffset = (record + 1) * sizeof(callbacks[0]);
            sTaskStateFailureAddress = native;
            return FALSE;
        }
        memcpy((u8 *)dest + record * sizeof(callbacks[0]), callbacks, sizeof(callbacks));
    }
    return TRUE;
#else
    (void)dest;
    return size == 0;
#endif
}

bool32 Task_ValidateState(const void *source, u32 size)
{
#if defined(LINUX64) && LINUX64
    u32 i;

    if (source == NULL || size != Task_GetStateSize())
        return FALSE;
    sTaskStateFailureOffset = 0;
    sTaskStateFailureAddress = 0;
    for (i = 0; i < NUM_TASKS * TASK_STATE_FUNCTIONS_PER_TASK; i++)
    {
        struct HostPersistentAddress callback;
        uintptr_t resolved;

        memcpy(&callback, (const u8 *)source + i * sizeof(callback), sizeof(callback));
        if (!HostResolvePersistentFunction(&callback, &resolved, sizeof(resolved)))
        {
            sTaskStateFailureOffset = i * sizeof(callback);
            memcpy(&sTaskStateFailureAddress, &callback, sizeof(callback));
            return FALSE;
        }
    }
    return TRUE;
#else
    (void)source;
    return size == 0;
#endif
}

bool32 Task_GetStateFailure(u32 *offset, uintptr_t *address)
{
#if defined(LINUX64) && LINUX64
    if (offset == NULL || address == NULL || sTaskStateFailureAddress == 0)
        return FALSE;
    *offset = sTaskStateFailureOffset;
    *address = sTaskStateFailureAddress;
    return TRUE;
#else
    (void)offset;
    (void)address;
    return FALSE;
#endif
}

bool32 Task_LoadState(const void *source, u32 size)
{
#if defined(LINUX64) && LINUX64
    u32 i;
    TaskFunc restoredFollowups[NUM_TASKS];
    unsigned char restoredStored[NUM_TASKS][sizeof(uintptr_t)];

    if (!Task_ValidateState(source, size))
        return FALSE;
    for (i = 0; i < NUM_TASKS; i++)
    {
        struct HostPersistentAddress callbacks[TASK_STATE_FUNCTIONS_PER_TASK];
        u32 record = i * TASK_STATE_FUNCTIONS_PER_TASK;

        memcpy(callbacks, (const u8 *)source + record * sizeof(callbacks[0]), sizeof(callbacks));
        if (!HostResolvePersistentFunction(&callbacks[0], &restoredFollowups[i],
                                           sizeof(restoredFollowups[i]))
         || !HostResolvePersistentFunction(&callbacks[1], restoredStored[i],
                                           sizeof(restoredStored[i])))
            return FALSE;
    }
    memcpy(sTaskFollowupFuncs, restoredFollowups, sizeof(restoredFollowups));
    memcpy(sTaskStoredFunctions, restoredStored, sizeof(restoredStored));
    return TRUE;
#else
    (void)source;
    return size == 0;
#endif
}
