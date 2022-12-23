#ifndef INT_HASH_H
#define INT_HASH_H

#include "allocator.h"

#include <stdint.h>
#include <time.h>

struct hashset_key;
typedef struct hashset_key {
	uint32_t key;
	struct hashset_key *bucket_next;
	struct hashset_key *ordered_next;
	struct hashset_key *ordered_prev;
} int_hashset_key;

typedef struct {
	allocator *allocator;
	uint32_t capacity;
	uint32_t length;
	int_hashset_key **buckets;
	int num_buckets;
	int_hashset_key *first;
	int_hashset_key *last;
	int_hashset_key *removed_key;
} int_hashset;

int_hashset *core_int_hashset_init(allocator *alloc, uint32_t num_buckets);

int core_int_hashset_add(int_hashset *hs, uint32_t key);

int core_int_hashset_contains(int_hashset *hs, uint32_t key);

int core_int_hashset_remove(int_hashset *hs, uint32_t key);

void core_int_hashset_iterate(int_hashset *hs, void *ctx, void(*callback)(void *ctx, int_hashset_key *hs_key));

#endif // INT_HASH_H
