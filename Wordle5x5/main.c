#include "allocator.h"
#include "int_hashset.h"

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#endif

#define BUFFER_SIZE 4300000
#define SUBMASK_BUCKETS 6
#define MAX_NUM_WORDS 5
#define WORD_LEN 5
#ifdef _WIN32
#define LINE_LENGTH 7 // line ends with \r\n
#define INPUT_FILE "C:/code/Wordle5x5C/Wordle5x5/words_alpha.txt"
#define OUTPUT_FILE "C:/code/Wordle5x5C/Wordle5x5/results_%d.txt"
#else
#define LINE_LENGTH 6 // line ends with \n
#define INPUT_FILE "/home/gordon/code/Wordle5x5C/Wordle5x5/words_alpha.txt"
#define OUTPUT_FILE "/home/gordon/code/Wordle5x5C/Wordle5x5/results_%d.txt"
#endif

typedef struct {
	int thread_num;
	int num_threads;
	pthread_t thread_id;
} parallel_parse_args;

typedef struct {
	uint32_t idx;
	uint32_t bits;
	uint32_t best_letter;
} full_word_info;

typedef struct {
	uint32_t idx;
	uint32_t bits;
} word_info;

typedef struct {
	int thread_num;
	int num_threads;
	pthread_t thread_id;
} parallel_solve_args;

static int num_cpu;
static char file_bytes[BUFFER_SIZE];
static size_t file_bytes_length;
static const char FREQUENCY_ALPHABET[26] = "qxjzvfwbkgpmhdcytlnuroisea";
static uint32_t frequency_alphabet_bits[26];
static char word_text[11000 * WORD_LEN];
static full_word_info word_infos[11000 * WORD_LEN];
static atomic_int word_count;

static full_word_info thread_words[1000];
static int thread_word_count;
static word_info letter_index[26][SUBMASK_BUCKETS][1000];
static int index_counts[26][SUBMASK_BUCKETS];

static uint32_t solutions[1000 * MAX_NUM_WORDS];
static atomic_int solution_count;

static int get_num_cpu()
{
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif __linux__
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

static void setup()
{
	num_cpu = get_num_cpu();
	//num_cpu = 1;
	for (int i = 0; i < 26; i++)
	{
		frequency_alphabet_bits[i] = 1 << (FREQUENCY_ALPHABET[i] - 97);
	}
	memset(word_text, 0, sizeof(word_text));
	word_count = 0;
	memset(thread_words, 0, sizeof(thread_words));
	thread_word_count = 0;
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

static void *parse_parallel(void *args)
{
	parallel_parse_args *p_args = (parallel_parse_args *)args;
	int bytes_to_process = file_bytes_length / p_args->num_threads;
	int file_idx = bytes_to_process * p_args->thread_num;
	if (p_args->thread_num == p_args->num_threads - 1) {
		bytes_to_process = file_bytes_length - file_idx;
	}
	int end_idx = file_idx + bytes_to_process;

	char buffer[5];
	char c = file_bytes[file_idx];
	if (p_args->thread_num > 0) {
		while (c != '\n') {
			file_idx--;
			c = file_bytes[file_idx];
		}
		file_idx++;
	}

	while (file_idx < end_idx) {
		int line_idx = 0;
		do {
			c = file_bytes[file_idx + line_idx];
			if (line_idx < 5)
				buffer[line_idx] = c;
			line_idx++;
		} while (c != '\n');
		file_idx += line_idx;
		if (file_idx > end_idx)
			break;
		if (line_idx != LINE_LENGTH)
			continue;

		uint32_t bits, best_letter;
		int is_valid = str_to_bits(buffer, &bits, &best_letter);
		if (!is_valid)
			continue;

		int last_word_count = atomic_fetch_add(&word_count, 1);
		memcpy(&word_text[last_word_count * WORD_LEN], buffer, WORD_LEN);
		full_word_info *wi = &word_infos[last_word_count];
		wi->bits = bits;
		wi->idx = last_word_count;
		wi->best_letter = best_letter;
	}
	
	return NULL;
}

static void read_file()
{
	clock_t start = clock();
	memset(file_bytes, 0, BUFFER_SIZE);
	FILE *fp = fopen(INPUT_FILE, "rb");
	file_bytes_length = fread(file_bytes, BUFFER_SIZE, 1, fp);
	fclose(fp);
	clock_t elapsed = clock() - start;
	printf("Read file time: %ld\n", elapsed);

	start = clock();
	parallel_parse_args thread_info[20];
	for (int i = 0; i < num_cpu; i++) {
		parallel_parse_args *p_args = &thread_info[i];
		p_args->thread_num = i;
		p_args->num_threads = num_cpu;
		pthread_create(&p_args->thread_id, NULL, parse_parallel, p_args);
	}
	for (int i = 0; i < num_cpu; i++) {
		pthread_join(thread_info[i].thread_id, NULL);
	}
	elapsed = clock() - start;
	printf("Parse file time: %ld\n", elapsed);

	start = clock();
	allocator *alloc = core_allocator_init(1, 400000);
	int_hashset *word_hashes = core_int_hashset_init(alloc, 39009);
	for (int i = 0; i < word_count; i++) {
		full_word_info fwi = word_infos[i];
		int added = core_int_hashset_add(word_hashes, fwi.bits);
		if (!added)
			continue;
		for (int j = 0; j < SUBMASK_BUCKETS; j++) {
			uint32_t submask = get_submask(j);
			if (j == SUBMASK_BUCKETS - 1 || (fwi.bits & submask) > 0) {
				int *index_count = &index_counts[fwi.best_letter][j];
				word_info *wi = &letter_index[fwi.best_letter][j][*index_count];
				wi->idx = fwi.idx;
				wi->bits = fwi.bits;
				*index_count = *index_count + 1;

				if (fwi.best_letter < 2) {
					thread_words[thread_word_count] = fwi;
					thread_word_count++;
				}
				break;
			}
		}
	}
	core_allocator_free_all(alloc);
	elapsed = clock() - start;
	printf("Index file time: %ld\n", elapsed);
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
}

static void solve_recursive(uint32_t bits, uint32_t *words_so_far, int letter_idx, int num_words, int num_skips)
{
	if (num_skips == 2)
		return;
	if (letter_idx == 26)
		return;
	if (num_words == MAX_NUM_WORDS) {
		int last_solution_count = atomic_fetch_add(&solution_count, 1);
		uint32_t *solution_offset = &solutions[last_solution_count * MAX_NUM_WORDS];
		memcpy(solution_offset, words_so_far, num_words * sizeof(uint32_t));
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

static void *solve_parallel(void *args)
{
	parallel_solve_args *p_args = (parallel_solve_args *)args;
	int thread_words_to_process = thread_word_count / p_args->num_threads;
	int thread_words_start = thread_words_to_process * p_args->thread_num;
	if (p_args->thread_num == p_args->num_threads - 1) {
		thread_words_to_process = thread_word_count - thread_words_start;
	}

	uint32_t words_so_far[5];
	memset(words_so_far, 0, sizeof(uint32_t));
	for (int i = 0; i < thread_words_to_process; i++) {
		full_word_info *fwi = &thread_words[thread_words_start + i];
		int letter_idx = fwi->best_letter + 1;
		int num_skips = fwi->best_letter;
		words_so_far[0] = fwi->idx;
		solve_recursive(fwi->bits, words_so_far, letter_idx, 1, num_skips);
	}
	return NULL;
}

static void solve(int iteration)
{
	clock_t start = clock();
	parallel_solve_args thread_info[20];
	for (int i = 0; i < num_cpu; i++) {
		parallel_solve_args *p_args = &thread_info[i];
		p_args->thread_num = i;
		p_args->num_threads = num_cpu;
		pthread_create(&p_args->thread_id, NULL, solve_parallel, p_args);
	}
	for (int i = 0; i < num_cpu; i++) {
		pthread_join(thread_info[i].thread_id, NULL);
	}
	clock_t elapsed = clock() - start;
	printf("Solve time: %ld\n", elapsed);

	start = clock();
	char output_file_name[100];
	sprintf(output_file_name, OUTPUT_FILE, iteration);
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
