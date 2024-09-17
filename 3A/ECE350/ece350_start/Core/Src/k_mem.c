#include "common.h"
#include "k_mem.h"
#include "k_task.h"
#include "main.h"
#include "stm32f4xx_it.h"
#include "kernel.h"

extern U32 _img_end; //the start of the heap will be from here, could add 0x200 bytes offset
extern U32 _estack;
extern U32 _Min_Stack_Size;
int k_mem_initialized = 0;
int counter[MAX_ORDER+1] = {0};
U32 heap_start = 0;
U32 heap_end = 0;
extern TCB* current_task;
extern int taskCreated;

header_block *header_array[MAX_ORDER+1] = {NULL};  // array of 11 pointers for the 11 levels; initially set to NULL

int k_mem_init(){
    // if the kernel has already been initialized
    if(initialized == 0 || k_mem_initialized == 1){
        return RTX_ERR;
        // if the kernel is not intialized or if the k mem init has already run once
    }
    else{
    	SHPR3 = (SHPR3 & ~(0xFFU << 24)) | (0xF0U << 24);//SysTick is lowest priority (highest number)
    	SHPR3 = (SHPR3 & ~(0xFFU << 16)) | (0xE0U << 16);//PendSV is in the middle
    	SHPR2 = (SHPR2 & ~(0xFFU << 24)) | (0xD0U << 24);//SVC is highest priority (lowest number)
        k_mem_initialized = 1;
        heap_start = (U32)&_img_end; //set the start of the heap to the end of the image
        heap_end = (U32)&_estack - (U32)&_Min_Stack_Size; //set the end of the heap to _estack - _Min_Stack_Size

        header_block *root_block = (header_block*)heap_start;       // create the very first free block encompassing all the free memory
        root_block->next = NULL;
        root_block->size = (1 << (MAX_ORDER + MIN_BLOCK_ORDER));
        root_block->status = 0;
        root_block->magic_number = MAGIC_NUMBER;

        counter[MAX_ORDER]++;
        //root_block->address = &heap_start+sizeof(header_block);

        header_array[MAX_ORDER] = root_block;       // place the root memory block at the last index of the pointer array
        //initialize the heap

        return RTX_OK;
    }
}

void *k_mem_alloc(size_t size){
    //allocates size bytes of memory according to the Buddy System algorithm and returns a pointer to the start of the usable memory in the block
	__disable_irq();
    if (k_mem_initialized == 0 || size == 0){
    	__enable_irq();
        return NULL;
    }
    else{
        // check what order the size fits within
        size += sizeof(header_block);   // add the header to the size
        int block_level = -1;
        for(int i = MIN_BLOCK_ORDER; i <= MIN_BLOCK_ORDER + MAX_ORDER; i++){       // find what memory block size is needed to accomodate
            if(size <= (1 << i)){
                block_level = i - MIN_BLOCK_ORDER;      // block order is now the index value needed to find blocks of the size we need
                break;
            }
        }
        if(block_level == -1){
            // we didn't find an appropriate size :(
        	__enable_irq();
            return NULL;
        }
        // check the header array at index block_level. this will be the list of free blocks or the desired size

        // if no node is found, the pointer in the array will be NULL. thus, we need to check the pointer value
        int tracker = block_level;
        while(header_array[tracker] == NULL){
            tracker++; // increment tracker to look at the level above
            if(tracker == MAX_ORDER + 1){
            	__enable_irq();
            	return NULL; // if we have reached the root and the pointer is NULL, there is simply no space for our current block
            }
        }
        // reach here once we find a list that has a free node we can split

        // now we need to split the free block until we reach our desired block size, block_level
        while(tracker != block_level){  // stay here until we have reached the block size we want
            // create 2 nodes in the level below
            header_block *buddy1 = header_array[tracker];
            header_block *buddy2 = (header_block*)((char*)header_array[tracker] + (1 << (tracker + MIN_BLOCK_ORDER - 1)));

            // remove the now-used node from the free list
            header_array[tracker] = (header_block*)header_array[tracker]->next;    // the pointer of the array now points to the next item in the free list.

            buddy1->size = (1 << (tracker + MIN_BLOCK_ORDER - 1));      //****
            buddy2->size = buddy1->size;

            //buddy1->address = header_array[tracker]->address;
            //buddy2->address = buddy1->address + buddy1->size;       // this is pointer arithmetic? need this checked

            buddy1->status = 0;
            buddy2->status = 0;

            buddy2->magic_number = MAGIC_NUMBER;

            buddy1->next = (struct header_block *) buddy2;
            buddy2->next = NULL;

            counter[tracker]--;
            // decrement tracker
            tracker--;
            // set the next free list
            counter[tracker]++;
            counter[tracker]++;
            header_array[tracker] = buddy1;
        }

        // now we have a free block we can manipulate

        header_array[block_level]->status = 1;
        if (taskCreated) header_array[block_level]->owner = taskCreated;
        else header_array[block_level]->owner = current_task->tid;
        taskCreated = 0;
        counter[tracker]--;
        void *pointer = (char*)header_array[block_level]+sizeof(header_block);
        header_array[block_level] = (header_block*)header_array[block_level]->next;
        __enable_irq();
        return pointer;
    }
}


int k_mem_dealloc(void *ptr){
	__disable_irq();
    if(k_mem_initialized == 0 || ptr == NULL){
    	__enable_irq();
        return RTX_ERR;
    }
    else{
        header_block *block_found = (header_block*)((char*)ptr - sizeof(header_block));
        // check the magic number of the header we found
        if(block_found->magic_number == MAGIC_NUMBER && block_found->owner == current_task->tid && block_found->status == 1){
            // we know that this header we found is something we allocated
            int block_level = 0;
            while(block_found->size != (1 << (block_level + MIN_BLOCK_ORDER))){
                block_level++;
            }
            // block level now found
            block_found->status = 0;

			if (header_array[block_level] > block_found || header_array[block_level] == NULL){
				block_found->next = (struct header_block *) header_array[block_level];
				header_array[block_level] = block_found;
			}
			else{
				block_found->next = header_array[block_level]->next;
				header_array[block_level]->next = (struct header_block *) block_found;
				block_found = header_array[block_level];
			}
            counter[block_level]++;

            int continue_coalescing = 1;
            while(continue_coalescing && block_level < 11){
                // find the buddy node
                header_block *buddy = (header_block*)((char*)block_found + (1 << (block_level + MIN_BLOCK_ORDER)));

                // check if the buddy is in use or not
                if(buddy->status == 0 && block_found->size < 32768 && block_found->size == buddy->size){
                    // need to find the element in the linked list
                    header_block *temp = header_array[block_level];
                    while(temp != NULL){
                        // iterate through the items in the list until we hit null
                        // only need to start checking from the second element since we inserted the just-freed block at the front of the LL
                        if((int)temp->next == (int)buddy){
                            // if the address of the buddy block we found matches an element in the array
                            temp->next = buddy->next;
                            break;
                            // the pointer of the node nows skips over the found buddy block, so the node is no longer in the free list
                        }
                        temp = (header_block *) temp->next;
                    }
                    // we should never reach here without finding the corresponding buddy node
                    // now we remove the recently added note
                    header_array[block_level] = (header_block *) header_array[block_level]->next;

                    counter[block_level]--;
                    counter[block_level]--;

                    // increment the block level to check if we need to coalesce at the parent level
                    block_level++;

                    // add the new free block to the LL above
                    block_found->next = (struct header_block *) header_array[block_level];
                    block_found->size <<= 1;
                    header_array[block_level] = block_found;

                    counter[block_level]++;
                    // insert the found block into the list above
                    // status is already set to 0
                }
                else{
                    // buddy is currently in use, stop coalescing
                    continue_coalescing = 0;
                }
            }
            // once we're here, coalescing has ceased and he have successfully inserted a free node into any lists that needed one
            __enable_irq();
            return RTX_OK;

        }
        else{
        	__enable_irq();
            return RTX_ERR;
            // return error since the magic number was not valid
        }
    }
}

int k_mem_count_extfrag(size_t size){
	__disable_irq();
	size_t temp = 32;
	int index = 0;
	int fragcount = 0;
	while (temp <= size){
		temp <<= 1;
		index++;
		if (index == 11) break;
	}
	for(int i = index - 1; i >= 0; i--){
		fragcount += counter[i];
	}
	__enable_irq();
	return fragcount;
}
