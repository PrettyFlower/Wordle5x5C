#ifndef CORE_ALLOCATION_BLOCK_H
#define CORE_ALLOCATION_BLOCK_H

#include <stdint.h>

struct alloc_block;
typedef struct alloc_block {
	uint8_t *mem_start;
	int allocated_bytes;
	int length;
	struct alloc_block *next_block;
} alloc_block;

int core_alloc_block_align_size(int size);

alloc_block *core_alloc_block_init(int size);

void *core_alloc_block_alloc(alloc_block *block, int size);

void core_alloc_block_free(alloc_block *block);

#endif // CORE_ALLOCATION_BLOCK_H
