/*
 * common.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: If you feel that there are common
 *      C functions corresponding to this
 *      header, then any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */
#include <stdint.h>
#include "main.h"
#ifndef INC_COMMON_H_
#define INC_COMMON_H_

typedef unsigned int U32;
typedef unsigned short U16;
typedef char U8;
typedef unsigned int task_t;

#define MAX_STACK_SIZE 0x4000
#define MAIN_STACK_SIZE 0x400
#define THREAD_STACK_SIZE 0x400

#define TID_NULL 0 //predefined Task ID for the NULL task
#define MAX_TASKS 16 //maximum number of tasks in the system1
#define STACK_SIZE 0x400 //min. size of each task’s stack
#define RTX_ERR -1 //error code for RTX functions
#define RTX_OK 0 //success code for RTX functions

#define MIN_BLOCK_ORDER 5	// min block size is 32 bytes
#define MIN_BLOCK_SIZE (1 << MIN_BLOCK_ORDER)
#define MAX_ORDER 10 // 11 levels deep
#define MAGIC_NUMBER 42069

#define DEFAULT_DEADLINE 5	// default deadline for a task is 5ms


typedef struct task_control_block {
	void (*ptask)(void *args); //entry address
	U32 *stack_high; //starting address of stack (high address)
	task_t tid; //task ID
	U8 state; //task's state; 0 -> dormant; 1 -> ready; 2 -> running; 3 -> sleeping
	U16 stack_size; //stack size. Must be a multiple of 8
	//your own fields at the end
	U32 *starting_address;
	int deadline; // deadline in ms that the task needs to complete by
	int remaining_time; // counts down from the deadline, needs to be reset upon yield
	int time_sleeping;
} TCB;

typedef struct header {
	U32 status;					// free/used, free = 0, used = 1
	U32 size;					// size of the total block of memory
	struct header_block *next;	// next free/occupied block, depending on which list it is for
	int magic_number;
	int owner;
} header_block;

#endif /* INC_COMMON_H_ */
