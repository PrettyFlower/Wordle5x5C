#ifndef CORE_ALLOCATOR_H
#define CORE_ALLOCATOR_H

#include "allocation_block.h"

typedef struct {
	int id;
	int block_size;
	alloc_block *first;
	alloc_block *last;
	int num_blocks;
	int track;
} allocator;

allocator *core_allocator_init(int id, int size);

void *core_allocator_alloc(allocator *alloc, int size);

void core_allocator_free_all(allocator *alloc);

#endif // CORE_ALLOCATOR_H
