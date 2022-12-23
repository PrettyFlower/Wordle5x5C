#include "allocation_block.h"

#include <stdlib.h>

int core_alloc_block_align_size(int size)
{
	// apparently it is undefined behavior if your pointers aren't aligned based on size?
	int ptr_size = sizeof(void *);
	int align_size = (size + ptr_size - 1) & -ptr_size;
	return align_size;
	//return size;
}

alloc_block *core_alloc_block_init(int size)
{
	int alloc_block_size = core_alloc_block_align_size(sizeof(alloc_block));
	if (size < alloc_block_size)
		return NULL;
	uint8_t *memory = (uint8_t *)calloc(1, size);
	if (memory == NULL)
		return NULL;
	alloc_block *block = (alloc_block *)memory;
	block->mem_start = memory;
	block->allocated_bytes = alloc_block_size;
	block->length = size;
	block->next_block = NULL;
	return block;
}

void *core_alloc_block_alloc(alloc_block *block, int size)
{
	int aligned_size = core_alloc_block_align_size(size);
	if (block->allocated_bytes + aligned_size > block->length)
		return NULL;
	void *memory = block->mem_start + block->allocated_bytes;
	block->allocated_bytes += aligned_size;
	return memory;
}

void core_alloc_block_free(alloc_block *block)
{
	free(block->mem_start);
}