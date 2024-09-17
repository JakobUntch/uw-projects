#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"

extern int initialized;

void osKernelInit(void);
int osKernelStart(void);
int osTaskInfo(task_t TID, TCB *task_copy);
task_t osGetTID(void);
int osTaskExit(void);

int osCreateTask(TCB *task);
void osYield(void);

int is_empty();

void osSleep(int time_in_ms);
void osPeriodYield();
int osSetDeadline(int deadline, task_t tid);
int osCreateDeadlineTask(int deadline, TCB* task);

#endif // KERNEL_H
