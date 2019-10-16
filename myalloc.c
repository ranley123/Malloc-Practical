/**
 * @author 170011474
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/mman.h>
#include "myalloc.h"
#include <pthread.h>
#include <unistd.h>

#ifndef MIN_ALLOC_UNIT
#define MIN_ALLOC_UNIT 3 * sysconf(_SC_PAGESIZE) // 3 pages
#endif

#ifndef STATUS_FREE
#define STATUS_FREE 1
#endif

#ifndef STATUS_NONFREE
#define STATUS_NONFREE 0
#endif

/**
 * a struct called block_t represents the type of header of a block
 * - stored in first 24 bytes
 */
typedef struct block
{
	int size;			// size requested by a user
	int free;			// free status
	struct block *prev; // the previous block
	struct block *next; // the next block
} block_t;

/**
 * Search for the specific block header to remove in the freelist
 * 
 * @param block_t block - the metadata header which should be removed from freelist
 * @return void
 */
void remove_block_from_freelist(block_t *block);

/**
 * add the specific block header into the freelist
 * 
 * @param block_t block - the metadata header which should be added into freelist
 * @return void
 */
void add_block_to_freelist(block_t *block);

/** 
 * search for a fixed size block in the free list
 * - the searched size = sizeof(block_t) + size requested by users
 * 
 * @param int size - the user wanted size
 * @return void * - a pointer pointing to the vacant block; return NULL if not found
 */
void *request_space_from_freelist(int size);

/**
 * request a fixed size block from the heap memory via mmap()
 * - the requested size = sizeof(block_t) + size requested by users
 * - if less than or equal to 3 pages, then a 3-page block will be allocated followed split()
 * - if larger than 3 pages, then a block with exact requested size will be allocated
 * 
 * @param int size - the user wanted size
 * @return void * - a pointer pointing to the newly allocated block; return NULL if fails
 */
void *request_space_from_heap(int size);

/**
 * split a large block into two blocks, one for returning back, another for remaining
 * - the address of the newly allocated block would be updated into the parameter block
 * - the address of the remaining block would be returned
 * 
 * @param block_t block - the large block waiting to be splited
 * @param int size - the size requested by the user
 * - the requested size = sizeof(block_t) + size requested by users
 * @return block_t block - the metadata block header waiting to be added into the freelist
 */
block_t *split(block_t *block, int size);

/**
 * obtain the starting address of user block based on the current block_t
 * - the starting address = the address of block_t block + sizeof(block)
 * 
 * @param block_t *block - the metadata header
 * @return void * - a void pointer pointing to the user block
 */
void *get_block_memory_start(block_t *block);

/**
 * obtain the starting address of header block_t based on the current pointer
 * - the starting address = ptr - sizeof(block)
 * 
 * @param void *ptr - pointer
 * @return void * - a void pointer pointing to the block header block_t
 */
void *get_block_header_start(void *ptr);

/**
 * merge two blocks if they are continuous free blocks in the freelist
 * - if starting address of block_t block + sizeof(block_t) + user size 
 * = starting address of the next block_t
 * - then two blocks are adjacent
 * 
 * @return void
 */
void merge_adjacent_blocks();

/**
 * a debugging method to show the current freelist information
 * - including information of each block in the iteration process
 * 
 * @return void
 */
void debug_show_freelist();

/**
 * a debugging method to show the curernt block information
 * - including the size, free status, the previous block, the next block
 * 
 * @return void
 */
void debug_show_block(block_t *block);

block_t *free_head = NULL;								  // the head of the free list
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER; // the global mutex
int freelist_size = 0;

void *myalloc(int size)
{
	if (size <= 0)
	{
		perror("Invalid memory size request: size need to be greater than 0 \n");
		return NULL;
	}

	void *ptr = NULL;
	int final_size = size + sizeof(block_t);

	pthread_mutex_lock(&global_mutex);

	// request a free block from the free list
	ptr = request_space_from_freelist(size);

	if (!ptr)
	{
		// if no available block in free list, then turn to heap
		ptr = request_space_from_heap(size);
		if (ptr == NULL)
			return NULL;
	}
	// set up 
	block_t *block = (block_t *)get_block_header_start(ptr);
	block->free = STATUS_NONFREE;
	
	debug_show_block(block);
	pthread_mutex_unlock(&global_mutex);
	return ptr;
}

void remove_block_from_freelist(block_t *block)
{
	if (!block->prev)
	{ // the block is the head
		if (block->next)
			free_head = block->next;
		else
			free_head = NULL;
	}
	else
		block->prev->next = block->next;
	if (block->next)
		block->next->prev = block->prev;
}

// need to maintain a sorted freelist based on the block address
// head address should be the largest
void add_block_to_freelist(block_t *block)
{
	// initialise
	block->prev = NULL;
	block->next = NULL;
	block->free = STATUS_FREE;

	// insert into a linked list sorted by address
	if (!free_head || (unsigned long)free_head > (unsigned long)block)
	{
		if (free_head)
			free_head->prev = block;
		block->next = free_head;
		free_head = block;
	}
	else
	{
		block_t *cur_block = free_head;
		// iterate until find the correct place
		while (cur_block->next && (unsigned long)cur_block->next < (unsigned long)block)
		{
			cur_block = cur_block->next;
		}
		block->next = cur_block->next;
		cur_block->next = block;
	}
}

block_t *split(block_t *block, int size)
{
	int final_size = size + sizeof(block_t);
	void *block_start = (void *)((unsigned long)block + sizeof(block_t));

	// set up remaining block
	block_t *remaining_block = (block_t *)((unsigned long)block_start + size);
	remaining_block->size = block->size - (final_size);

	// set up the block to be returned to mmap()
	block->size = size;

	return remaining_block;
}

void merge_adjacent_blocks()
{
	block_t *cur_block = free_head;

	while (cur_block->next)
	{
		unsigned long cur_header = (unsigned long)cur_block;
		unsigned long next_header = (unsigned long)cur_block->next;

		// the case where should do merging
		if (next_header == cur_header + cur_block->size + sizeof(block_t))
		{
			cur_block->size += cur_block->next->size + sizeof(block_t);
			cur_block->next = cur_block->next->next;
			if (cur_block->next)
				cur_block->next->prev = cur_block;
			else
				break;
		}
		else
			cur_block = cur_block->next;
	}
}

void myfree(void *ptr)
{
	if (ptr == NULL)
		return;
	block_t *block = (block_t *)get_block_header_start(ptr);
	if (block->free == STATUS_FREE)
	{
		perror("Free Failed: double free \n");
		return;
	}

	pthread_mutex_lock(&global_mutex);

	add_block_to_freelist(block);
	merge_adjacent_blocks();
	debug_show_freelist();
	pthread_mutex_unlock(&global_mutex);
}

void *request_space_from_freelist(int size)
{
	block_t *block;
	int final_size = size + sizeof(block_t);
	block_t *cur_block = free_head;

	while (cur_block)
	{
		if (cur_block->size < final_size)
			cur_block = cur_block->next;
		else
		{
			remove_block_from_freelist(cur_block);
			if (cur_block->size == final_size)
			{
				return get_block_memory_start(cur_block);
			}
			block_t *remaining_block = split(cur_block, size);
			add_block_to_freelist(remaining_block);
			return get_block_memory_start(cur_block);
		}
	}
	return NULL;
}

void *request_space_from_heap(int size)
{
	int final_size = size + sizeof(block_t);

	// if requested size is larger than MIN_ALLOC_UNIT, then alloc_size should be the requested size
	int alloc_size = size >= MIN_ALLOC_UNIT ? final_size : MIN_ALLOC_UNIT;
	block_t *new_block = mmap(0, alloc_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

	if (new_block == MAP_FAILED)
	{
		fprintf(stdout, "Allocation failded size: %ld", MIN_ALLOC_UNIT);
		return NULL;
	}

	// set up
	new_block->next = NULL;
	new_block->prev = NULL;
	new_block->size = alloc_size - sizeof(block_t);
	
	if (new_block->size > final_size)
	{
		block_t *remaining_block = split(new_block, size);
		add_block_to_freelist(remaining_block);
	}

	return get_block_memory_start(new_block);
}

void *get_block_memory_start(block_t *block)
{
	return ((void *)((unsigned long)block + sizeof(block_t)));
}

void *get_block_header_start(void *ptr)
{
	return ((void *)((unsigned long)ptr - sizeof(block_t)));
}

void debug_show_block(block_t *block)
{
	fprintf(stdout, "==================================================== \nBlock Information \n");
	void *ptr = get_block_memory_start(block);
	fprintf(stdout, "Block header starting address: %lu \n", (unsigned long)block);
	fprintf(stdout, "Block userspace starting address: %lu \n", (unsigned long)ptr);
	fprintf(stdout, "Block userspace size: %d\n", block->size);
	fprintf(stdout, "Block free status: %d \n", block->free);
	fprintf(stdout, "==================================================== \n");
}

void debug_show_freelist()
{
	fprintf(stdout, "====================================================\n");
	fprintf(stdout, "Free List Debug Information \n");

	block_t *cur_block = free_head;
	while (cur_block)
	{
		debug_show_block(cur_block);
		cur_block = cur_block->next;
	}
	fprintf(stdout, "\n");
}