#ifndef GUARD_TASK_H
#define GUARD_TASK_H

#define HEAD_SENTINEL 0xFE
#define TAIL_SENTINEL 0xFF
#define TASK_NONE TAIL_SENTINEL

#define NUM_TASKS 16
#define NUM_TASK_DATA 16
#define TASK_STATE_FUNCTIONS_PER_TASK 2

typedef void (*TaskFunc)(u8 taskId);

struct Task
{
    TaskFunc func;
    bool8 isActive;
    u8 prev;
    u8 next;
    u8 priority;
    s16 data[NUM_TASK_DATA];
};

extern struct Task gTasks[];

void ResetTasks(void);
u8 CreateTask(TaskFunc func, u8 priority);
void DestroyTask(u8 taskId);
void RunTasks(void);
void TaskDummy(u8 taskId);
void SetTaskFuncWithFollowupFunc(u8 taskId, TaskFunc func, TaskFunc followupFunc);
void SwitchTaskToFollowupFunc(u8 taskId);
bool8 FuncIsActiveTask(TaskFunc func);
u8 FindTaskIdByFunc(TaskFunc func);
u8 GetTaskCount(void);
void SetWordTaskArg(u8 taskId, u8 dataElem, u32 value);
u32 GetWordTaskArg(u8 taskId, u8 dataElem);
u32 Task_GetStateSize(void);
bool32 Task_SaveState(void *dest, u32 size);
bool32 Task_ValidateState(const void *source, u32 size);
bool32 Task_LoadState(const void *source, u32 size);
bool32 Task_GetStateFailure(u32 *offset, uintptr_t *address);
#if defined(LINUX64) && LINUX64
void Task_StoreFunction(u8 taskId, const void *functionPointerBytes, u32 size);
void Task_LoadStoredFunction(u8 taskId, void *functionPointerBytes, u32 size);
#endif

#endif // GUARD_TASK_H
