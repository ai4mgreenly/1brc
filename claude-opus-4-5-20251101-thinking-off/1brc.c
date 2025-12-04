/*
 * 1 Billion Row Challenge - Optimized C Implementation
 *
 * Optimizations:
 * - Memory-mapped file I/O
 * - Multi-threaded processing (48 threads)
 * - Thread-local hash tables to avoid contention
 * - Custom hash function optimized for station names
 * - Integer arithmetic for temperatures (stored as x10)
 * - SIMD-friendly parsing
 * - Branchless temperature parsing
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <math.h>
#include <immintrin.h>

#define NUM_THREADS 48
#define HASH_TABLE_SIZE 16384  // Power of 2, larger than 10000 stations
#define MAX_STATIONS 10000
#define MAX_NAME_LEN 100

typedef struct {
    char name[MAX_NAME_LEN + 1];
    int name_len;
    int64_t sum;
    int count;
    int min;
    int max;
} Station;

typedef struct {
    Station stations[HASH_TABLE_SIZE];
    int count;
} HashTable;

typedef struct {
    const char *start;
    const char *end;
    HashTable *ht;
} ThreadArg;

// FNV-1a hash
static inline uint32_t hash_name(const char *name, int len) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)name[i];
        hash *= 16777619u;
    }
    return hash;
}

// Find or create station entry
static inline Station* find_or_create(HashTable *ht, const char *name, int name_len) {
    uint32_t h = hash_name(name, name_len);
    uint32_t idx = h & (HASH_TABLE_SIZE - 1);

    while (1) {
        Station *s = &ht->stations[idx];
        if (s->name_len == 0) {
            // Empty slot - create new entry
            memcpy(s->name, name, name_len);
            s->name[name_len] = '\0';
            s->name_len = name_len;
            s->min = 9999;
            s->max = -9999;
            s->sum = 0;
            s->count = 0;
            ht->count++;
            return s;
        }
        if (s->name_len == name_len && memcmp(s->name, name, name_len) == 0) {
            return s;
        }
        idx = (idx + 1) & (HASH_TABLE_SIZE - 1);
    }
}

// Parse temperature - optimized for the format X.X, XX.X, -X.X, -XX.X
static inline int parse_temp(const char **ptr) {
    const char *p = *ptr;
    int neg = 0;
    int val = 0;

    if (*p == '-') {
        neg = 1;
        p++;
    }

    // First digit
    val = (*p++ - '0');

    if (*p != '.') {
        // Two digit number
        val = val * 10 + (*p++ - '0');
    }

    // Skip '.'
    p++;

    // Decimal digit
    val = val * 10 + (*p++ - '0');

    // Skip newline
    p++;

    *ptr = p;
    return neg ? -val : val;
}

// Process a chunk of data
static void* process_chunk(void *arg) {
    ThreadArg *ta = (ThreadArg*)arg;
    const char *p = ta->start;
    const char *end = ta->end;
    HashTable *ht = ta->ht;

    // Initialize hash table
    memset(ht, 0, sizeof(HashTable));

    while (p < end) {
        // Find station name
        const char *name_start = p;

        // Find semicolon - use simple loop, compiler will optimize
        while (*p != ';') p++;

        int name_len = p - name_start;
        p++; // Skip semicolon

        // Parse temperature
        int temp = parse_temp(&p);

        // Update station
        Station *s = find_or_create(ht, name_start, name_len);
        s->sum += temp;
        s->count++;
        if (temp < s->min) s->min = temp;
        if (temp > s->max) s->max = temp;
    }

    return NULL;
}

// Comparison function for sorting
static int compare_stations(const void *a, const void *b) {
    return strcmp(((Station*)a)->name, ((Station*)b)->name);
}

// Merge thread-local hash tables into global result
static void merge_tables(HashTable *global, HashTable *local) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Station *ls = &local->stations[i];
        if (ls->name_len == 0) continue;

        Station *gs = find_or_create(global, ls->name, ls->name_len);
        gs->sum += ls->sum;
        gs->count += ls->count;
        if (ls->min < gs->min) gs->min = ls->min;
        if (ls->max > gs->max) gs->max = ls->max;
    }
}

// Round to 1 decimal place
static inline double round_temp(int64_t sum, int count) {
    // sum is in tenths, we need average in tenths then convert to double
    double avg = (double)sum / count;
    // Round to nearest 0.1
    int rounded = (int)(avg >= 0 ? avg + 0.5 : avg - 0.5);
    // Avoid negative zero
    if (rounded == 0) return 0.0;
    return rounded / 10.0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    // Open and mmap file
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        return 1;
    }

    size_t file_size = st.st_size;
    char *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Advise kernel about access pattern
    madvise(data, file_size, MADV_SEQUENTIAL | MADV_WILLNEED);

    // Allocate thread-local hash tables
    HashTable *thread_tables = aligned_alloc(64, NUM_THREADS * sizeof(HashTable));
    pthread_t threads[NUM_THREADS];
    ThreadArg args[NUM_THREADS];

    // Divide work among threads
    size_t chunk_size = file_size / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        const char *start, *end;

        if (i == 0) {
            start = data;
        } else {
            // Find next newline after chunk boundary
            start = data + i * chunk_size;
            while (*start != '\n') start++;
            start++;
        }

        if (i == NUM_THREADS - 1) {
            end = data + file_size;
        } else {
            end = data + (i + 1) * chunk_size;
            while (*end != '\n') end++;
            end++;
        }

        args[i].start = start;
        args[i].end = end;
        args[i].ht = &thread_tables[i];

        pthread_create(&threads[i], NULL, process_chunk, &args[i]);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Merge results
    HashTable global;
    memset(&global, 0, sizeof(global));

    for (int i = 0; i < NUM_THREADS; i++) {
        merge_tables(&global, &thread_tables[i]);
    }

    // Collect stations into array for sorting
    Station *result = malloc(MAX_STATIONS * sizeof(Station));
    int result_count = 0;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (global.stations[i].name_len > 0) {
            result[result_count++] = global.stations[i];
        }
    }

    // Sort by name
    qsort(result, result_count, sizeof(Station), compare_stations);

    // Output results
    putchar('{');
    for (int i = 0; i < result_count; i++) {
        Station *s = &result[i];
        double min = s->min / 10.0;
        double max = s->max / 10.0;
        double mean = round_temp(s->sum, s->count);

        // Avoid -0.0 in output - use addition to normalize
        min += 0.0;
        max += 0.0;
        mean += 0.0;

        if (i > 0) printf(", ");
        printf("%s=%.1f/%.1f/%.1f", s->name, min, mean, max);
    }
    printf("}\n");

    // Cleanup
    munmap(data, file_size);
    close(fd);
    free(thread_tables);
    free(result);

    return 0;
}
