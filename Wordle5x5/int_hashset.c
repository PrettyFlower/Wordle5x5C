#include "int_hashset.h"
#include "prime_utils.h"

#include <string.h>

static int get_num_buckets(int capacity)
{
	if (capacity >= UINT32_MAX)
		return UINT32_MAX;
	int num_buckets = core_prime_utils_get_next_prime(capacity);
	return num_buckets;
}

static void resize(int_hashset *hs) {
	uint32_t new_capacity = 2 * hs->capacity;
	new_capacity = get_num_buckets(new_capacity);
	if (new_capacity <= hs->capacity)
		return;

	int_hashset_key **new_buckets = core_allocator_alloc(hs->allocator, sizeof(int_hashset_key *) * new_capacity);

	int_hashset_key *next = hs->first;
	while (next) {
		int new_bucket = next->key % new_capacity;
		next->bucket_next = new_buckets[new_bucket];
		new_buckets[new_bucket] = next;
		next = next->ordered_next;
	}

	hs->buckets = new_buckets;
	hs->capacity = new_capacity;
	hs->num_buckets = new_capacity;
}

int_hashset *core_int_hashset_init(allocator *alloc, uint32_t initial_capacity)
{
	int_hashset *hs = core_allocator_alloc(alloc, sizeof(int_hashset));
	hs->allocator = alloc;
	hs->capacity = initial_capacity > 0 ? initial_capacity : 1;
	hs->length = 0;
	hs->num_buckets = get_num_buckets(initial_capacity);
	hs->buckets = core_allocator_alloc(hs->allocator, sizeof(int_hashset_key *) * hs->num_buckets);
	hs->first = NULL;
	hs->last = NULL;
	hs->removed_key = NULL;
	return hs;
}

int core_int_hashset_add(int_hashset *hs, uint32_t key)
{
	if (hs->length >= hs->capacity)
		resize(hs);

	// find the right hash bucket with the linked list of KVPs
	int bucket = key % hs->num_buckets;
	int_hashset_key *existing = hs->buckets[bucket];

	// find the end of the linked list
	while (existing) {
		// we already have a matching key, so return
		if (existing->key == key) {
			return 0;
		}
		existing = existing->bucket_next;
	}

	// get a new KVP to store the data
	int_hashset_key *hs_key;
	// check to see if we have any deleted KVPs hanging around we can re-use
	if (hs->removed_key) {
		hs_key = hs->removed_key;
		hs->removed_key = hs_key->bucket_next;
	}
	else {
		hs_key = core_allocator_alloc(hs->allocator, sizeof(int_hashset_key));
	}
	hs_key->key = key;

	// update the bucket linked list
	hs_key->bucket_next = hs->buckets[bucket];
	hs->buckets[bucket] = hs_key;

	// update the sorted linked list
	if (hs->last != NULL)
		hs->last->ordered_next = hs_key;
	hs_key->ordered_prev = hs->last;

	// update the dictionary values
	if (hs->first == NULL)
		hs->first = hs_key;
	hs->last = hs_key;
	hs->length++;
	return 1;
}

int core_int_hashset_contains(int_hashset *hs, uint32_t key)
{
	int bucket = key % hs->num_buckets;
	int_hashset_key *hs_key = hs->buckets[bucket];
	while (hs_key != NULL) {
		if (hs_key->key == key) {
			return 1;
		}
		hs_key = hs_key->bucket_next;
	}
	return 0;
}

int core_int_hashset_remove(int_hashset *hs, uint32_t key)
{
	int bucket = key % hs->num_buckets;
	int_hashset_key *hs_key = hs->buckets[bucket];
	int_hashset_key *last_bucket_key = NULL;
	while (hs_key != NULL) {
		if (hs_key->key == key) {
			// remove from bucket and reconnect the linked list
			if (last_bucket_key)
				last_bucket_key->bucket_next = hs_key->bucket_next;
			else
				hs->buckets[bucket] = hs_key->bucket_next;

			// update dict values
			if (hs->first == hs_key)
				hs->first = hs_key->ordered_next;
			if (hs->last == hs_key)
				hs->last = hs_key->ordered_prev;
			hs->length--;

			// remove from ordered linked list and reconnect it
			if (hs_key->ordered_next)
				hs_key->ordered_next->ordered_prev = hs_key->ordered_prev;
			if (hs_key->ordered_prev)
				hs_key->ordered_prev->ordered_next = hs_key->ordered_next;

			// zero out and add to list of removed KVPs
			memset(hs_key, 0, sizeof(int_hashset_key));
			hs_key->bucket_next = hs->removed_key;
			hs->removed_key = hs_key;
			return 1;
		}
		last_bucket_key = hs_key;
		hs_key = hs_key->bucket_next;
	}
	return 0;
}

void core_dict_iterate(int_hashset *hs, void *ctx, void(*callback)(void *ctx, int_hashset_key *hs_key))
{
	int_hashset_key *next = hs->first;
	while (next) {
		callback(ctx, next);
		next = next->ordered_next;
	}
}