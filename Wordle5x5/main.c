#include "allocator.h"
#include "int_hashset.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4300000
#define SUBMASK_BUCKETS 6
#define MAX_NUM_WORDS 5
#define WORD_LEN 5

typedef struct {
	uint32_t idx;
	uint32_t bits;
} word_info;

static const char FREQUENCY_ALPHABET[26] = "qxjzvfwbkgpmhdcytlnuroisea";
static uint32_t frequency_alphabet_bits[26];
static char word_text[6000 * WORD_LEN];
static int word_count;
static word_info letter_index[26][SUBMASK_BUCKETS][500];
static int index_counts[26][SUBMASK_BUCKETS];
static uint32_t solutions[1000 * MAX_NUM_WORDS];
static int solution_count;

static void setup()
{
	for (int i = 0; i < 26; i++)
	{
		frequency_alphabet_bits[i] = 1 << (FREQUENCY_ALPHABET[i] - 97);
	}
	memset(word_text, 0, sizeof(word_text));
	word_count = 0;
	memset(letter_index, 0, sizeof(letter_index));
	memset(index_counts, 0, sizeof(index_counts));
	memset(solutions, 0, sizeof(solutions));
	solution_count = 0;
}

static uint32_t get_submask(int i)
{
	return frequency_alphabet_bits[26 - i - 1];
}

static int str_to_bits(char *str, uint32_t *bits, uint32_t *best_letter)
{
	*bits = 0;
	for (int i = 0; i < WORD_LEN; i++) {
		char bit_offset = str[i] - 97;
		int bit = 1 << bit_offset;
		if ((*bits & bit) > 0)
			return 0;
		*bits |= bit;
	}
	for (int i = 0; i < 26; i++) {
		if ((*bits & frequency_alphabet_bits[i]) > 0) {
			*best_letter = i;
			break;
		}
	}
	return 1;
}

static void read_file()
{
	clock_t start = clock();
	FILE *fp = fopen("/home/gordon/code/Wordle5x5C/Wordle5x5/words_alpha.txt", "rb");
	char *file_bytes = calloc(1, BUFFER_SIZE);
	size_t num_file_bytes = fread(file_bytes, 1, BUFFER_SIZE, fp);
	fclose(fp);

	char buffer[5];
	int file_idx = 0;
	char c = file_bytes[file_idx];
	allocator *alloc = core_allocator_init(1, 400000);
	int_hashset *word_hashes = core_int_hashset_init(alloc, 39009);
	while (file_idx < num_file_bytes) {
		int line_idx = 0;
		do {
			c = file_bytes[file_idx + line_idx];
			if (line_idx < 5)
				buffer[line_idx] = c;
			line_idx++;
		} while (c != '\n');
		file_idx += line_idx;
		if (line_idx != 6)
			continue;

		uint32_t bits, best_letter;
		int is_valid = str_to_bits(buffer, &bits, &best_letter);
		if (!is_valid)
			continue;
		int added = core_int_hashset_add(word_hashes, bits);
		if(!added)
			continue;
		for (int i = 0; i < SUBMASK_BUCKETS; i++) {
			uint32_t submask = get_submask(i);
			if (i == SUBMASK_BUCKETS - 1 || (bits & submask) > 0) {
				memcpy(&word_text[word_count * WORD_LEN], buffer, WORD_LEN);
				int *index_count = &index_counts[best_letter][i];
				word_info *wi = &letter_index[best_letter][i][*index_count];
				wi->bits = bits;
				wi->idx = word_count;
				*index_count = *index_count + 1;
				word_count++;
				break;
			}
		}
	}
	free(file_bytes);
	core_allocator_free_all(alloc);

	clock_t elapsed = clock() - start;
	printf("Parse file time: %ld\n", elapsed);
}

static void idx_to_word(char *buffer, uint32_t idx)
{
	memcpy(buffer, &word_text[idx * WORD_LEN], WORD_LEN);
	buffer[WORD_LEN] = '\0';
}

static void idxs_to_solution(char *buffer, uint32_t *words_so_far, int num_words)
{
	memset(buffer, 0, num_words * 6 + 1);
	for (int i = 0; i < num_words; i++) {
		uint32_t word_idx = words_so_far[i];
		char *word = &word_text[word_idx * WORD_LEN];
		memcpy(&buffer[i * 6], word, WORD_LEN);
		buffer[i * 6 + WORD_LEN] = ' ';
	}
	//buffer[num_words * 7] = '\0';
}

static void idx_to_solution(char *buffer, uint32_t solution, int num_words)
{
	memset(buffer, 0, num_words * 6 + 1);
	for (int i = 0; i < num_words; i++) {
		uint32_t word_idx = solutions[(solution * MAX_NUM_WORDS) + i];
		char *word = &word_text[word_idx * WORD_LEN];
		memcpy(&buffer[i * 6], word, WORD_LEN);
		buffer[i * 6 + WORD_LEN] = ' ';
	}
	//buffer[num_words * 7] = '\0';
}

static void solve_recursive(uint32_t bits, uint32_t *words_so_far, int letter_idx, int num_words, int num_skips)
{
	if (num_skips == 2)
		return;
	if (letter_idx == 26)
		return;
	if (num_words == MAX_NUM_WORDS) {
		uint32_t *solution_offset = &solutions[solution_count * MAX_NUM_WORDS];
		memcpy(solution_offset, words_so_far, num_words * sizeof(uint32_t));
		solution_count++;
		return;
	}

	if ((bits & frequency_alphabet_bits[letter_idx]) == 0) {
		for (int i = 0; i < SUBMASK_BUCKETS; i++) {
			uint32_t submask = get_submask(i);
			if (i == SUBMASK_BUCKETS - 1 || (bits & submask) == 0) {
				for (int j = 0; j < index_counts[letter_idx][i]; j++) {
					word_info wi = letter_index[letter_idx][i][j];
					if ((wi.bits & bits) > 0)
						continue;
					words_so_far[num_words] = wi.idx;
					uint32_t new_bits = bits | wi.bits;
					solve_recursive(new_bits, words_so_far, letter_idx + 1, num_words + 1, num_skips);
				}
			}
		}
		solve_recursive(bits, words_so_far, letter_idx + 1, num_words, num_skips + 1);
	} else {
		solve_recursive(bits, words_so_far, letter_idx + 1, num_words, num_skips);
	}
}

static void solve(int iteration)
{
	clock_t start = clock();
	uint32_t words_so_far[5];
	memset(words_so_far, 0, sizeof(uint32_t));
	solve_recursive(0, words_so_far, 0, 0, 0);
	clock_t elapsed = clock() - start;
	printf("Solve time: %ld\n", elapsed);

	start = clock();
	char output_file_name[100];
	sprintf(output_file_name, "/home/gordon/code/Wordle5x5C/Wordle5x5/results_%d.txt", iteration);
	FILE *output = fopen(output_file_name, "wb");
	/*if (err != 0) {
		printf("Error opening file for writing: %d, %ld\n", err, _doserrno);
		return;
	}*/
	for (int i = 0; i < solution_count; i++) {
		char buffer[MAX_NUM_WORDS * 6 + 1];
		idx_to_solution(buffer, i, MAX_NUM_WORDS);
		fwrite(buffer, 1, MAX_NUM_WORDS * 6 - 1, output);
		fwrite("\n", 1, 1, output);
	}
	fclose(output);
	elapsed = clock() - start;
	printf("Write time: %ld\n", elapsed);
}

int main()
{
	for (int i = 0; i < 1; i++) {
		clock_t start = clock();
		setup();
		read_file();
		solve(i);
		clock_t elapsed = clock() - start;
		printf("Total time: %ld\n", elapsed);
		printf("\n");
	}
	return 0;
}
