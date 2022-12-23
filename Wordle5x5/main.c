#include "allocator.h"
#include "int_hashset.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4300000
#define SUBMASK_BUCKETS 6
#define WORD_LEN 5

static const char FREQUENCY_ALPHABET[26] = "qxjzvfwbkgpmhdcytlnuroisea";
static uint32_t frequency_alphabet_bits[26];
static char word_text[26][SUBMASK_BUCKETS][500 * WORD_LEN];
static uint32_t word_bits[26][SUBMASK_BUCKETS][500];
static int word_counts[26][SUBMASK_BUCKETS];
static char solutions[1000 * WORD_LEN * WORD_LEN];
static int solution_count;

typedef struct {
	int letter;
	int submask_bucket;
	int idx;
} word_ptr;

static void setup()
{
	for (int i = 0; i < 26; i++)
	{
		frequency_alphabet_bits[i] = 1 << (FREQUENCY_ALPHABET[i] - 97);
	}
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
	FILE *fp;
	fopen_s(&fp, "C:/code/Wordle5x5/Wordle5x5/words_alpha.txt", "rb");
	char *file_bytes = calloc(1, BUFFER_SIZE);
	size_t num_file_bytes = fread_s(file_bytes, BUFFER_SIZE, 1, BUFFER_SIZE, fp);
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
		if (line_idx != 7)
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
				int num_words = word_counts[best_letter][i];
				memcpy(&word_text[best_letter][i][num_words * WORD_LEN], buffer, 5);
				word_bits[best_letter][i][num_words] = bits;
				word_counts[best_letter][i] = num_words + 1;
				break;
			}
		}
	}
	free(file_bytes);
	core_allocator_free_all(alloc);

	clock_t elapsed = clock() - start;
	printf("Parse file time: %ld\n", elapsed);
}

static void solve_recursive(uint32_t bits, word_ptr *words_so_far, int letter_idx, int num_words, int num_skips)
{
	if (num_skips == 2)
		return;
	if (letter_idx == 26)
		return;
	if (num_words == WORD_LEN) {
		for (int i = 0; i < WORD_LEN; i++) {
			word_ptr *word_ptr = &words_so_far[i];
			char *solution_offset = &solutions[solution_count * WORD_LEN * WORD_LEN + i * WORD_LEN];
			char *word = &word_text[word_ptr->letter][word_ptr->submask_bucket][word_ptr->idx * WORD_LEN];
			memcpy(solution_offset, word, WORD_LEN);
		}
		solution_count++;
		return;
	}

	if ((bits & frequency_alphabet_bits[letter_idx]) == 0) {
		for (int i = 0; i < SUBMASK_BUCKETS; i++) {
			uint32_t submask = get_submask(i);
			if (i == SUBMASK_BUCKETS - 1 || (bits & submask) == 0) {
				for (int j = 0; j < word_counts[letter_idx][i]; j++) {
					uint32_t word_bitmask = word_bits[letter_idx][i][j];
					if ((word_bitmask & bits) > 0)
						continue;
					word_ptr *ptr = &words_so_far[num_words];
					ptr->letter = letter_idx;
					ptr->submask_bucket = i;
					ptr->idx = j;
					uint32_t new_bits = bits | word_bitmask;
					solve_recursive(new_bits, words_so_far, letter_idx + 1, num_words + 1, num_skips);
				}
			}
		}
		solve_recursive(bits, words_so_far, letter_idx + 1, num_words, num_skips + 1);
	} else {
		solve_recursive(bits, words_so_far, letter_idx + 1, num_words, num_skips);
	}
}

static void solve()
{
	clock_t start = clock();
	word_ptr words_so_far[5];
	memset(words_so_far, 0, sizeof(words_so_far));
	solve_recursive(0, words_so_far, 0, 0, 0);
	FILE *output;
	fopen_s(&output, "C:/code/Wordle5x5/Wordle5x5/results.txt", "wb");
	for (int i = 0; i < solution_count; i++) {
		for (int j = 0; j < WORD_LEN; j++) {
			fwrite(&solutions[i * WORD_LEN * WORD_LEN + j * WORD_LEN], WORD_LEN, 1, output);
			fwrite(" ", 1, 1, output);
		}
		fwrite("\n", 1, 1, output);
	}
	fclose(output);
	memset(solutions, 0, sizeof(solutions));
	solution_count = 0;
	clock_t elapsed = clock() - start;
	printf("Solve time: %ld\n", elapsed);
}

int main()
{
	setup();
	read_file();
	solve();
	return 0;
}
