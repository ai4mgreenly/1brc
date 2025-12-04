#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <immintrin.h>
#include <math.h>

#define MAX_STATIONS 10000
#define HASH_SIZE 16384  // Power of 2 for fast modulo
#define MAX_NAME_LEN 100

typedef struct {
    char name[MAX_NAME_LEN + 1];
    int64_t sum;
    int32_t count;
    int16_t min;
    int16_t max;
    uint32_t hash;
    int name_len;
} Station;

typedef struct {
    Station stations[MAX_STATIONS];
    int count;
    Station* hash_table[HASH_SIZE];
} StationMap;

// Fast hash function optimized for short strings
static inline uint32_t hash_name(const char* name, int len) {
    uint32_t hash = 5381;
    for (int i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)name[i];
    }
    return hash;
}

// Parse temperature as fixed-point (multiplied by 10)
static inline int16_t parse_temp(const char* p, const char** end) {
    int16_t result = 0;
    int negative = 0;

    if (*p == '-') {
        negative = 1;
        p++;
    }

    // Parse integer part
    if (p[1] == '.') {
        // Single digit
        result = (p[0] - '0') * 10;
        result += p[2] - '0';
        *end = p + 3;
    } else {
        // Two digits
        result = (p[0] - '0') * 100 + (p[1] - '0') * 10;
        result += p[3] - '0';
        *end = p + 4;
    }

    return negative ? -result : result;
}

// Find or insert station
static inline Station* get_station(StationMap* map, const char* name, int name_len, uint32_t hash) {
    uint32_t idx = hash & (HASH_SIZE - 1);

    // Linear probing
    while (1) {
        Station* s = map->hash_table[idx];
        if (s == NULL) {
            // New station
            s = &map->stations[map->count++];
            s->hash = hash;
            s->name_len = name_len;
            memcpy(s->name, name, name_len);
            s->name[name_len] = '\0';
            s->sum = 0;
            s->count = 0;
            s->min = 32767;
            s->max = -32768;
            map->hash_table[idx] = s;
            return s;
        }

        if (s->hash == hash && s->name_len == name_len &&
            memcmp(s->name, name, name_len) == 0) {
            return s;
        }

        idx = (idx + 1) & (HASH_SIZE - 1);
    }
}

// Update station stats
static inline void update_station(Station* s, int16_t temp) {
    s->sum += temp;
    s->count++;
    if (temp < s->min) s->min = temp;
    if (temp > s->max) s->max = temp;
}

// Process a chunk of data
static void process_chunk(StationMap* map, const char* data, size_t size) {
    const char* p = data;
    const char* end = data + size;

    while (p < end) {
        // Find semicolon
        const char* name_start = p;
        const char* semicolon = p;
        while (*semicolon != ';') semicolon++;

        int name_len = semicolon - name_start;
        uint32_t hash = hash_name(name_start, name_len);

        // Parse temperature
        const char* temp_end;
        int16_t temp = parse_temp(semicolon + 1, &temp_end);

        // Get or create station
        Station* station = get_station(map, name_start, name_len, hash);
        update_station(station, temp);

        // Skip to next line
        p = temp_end + 1;
    }
}

// Compare function for qsort
static int compare_stations(const void* a, const void* b) {
    const Station* s1 = *(const Station**)a;
    const Station* s2 = *(const Station**)b;
    return strcmp(s1->name, s2->name);
}

// Round value to 1 decimal place and fix negative zero
static inline double round_temp(double temp) {
    // Round to 1 decimal place
    double rounded = round(temp * 10.0) / 10.0;
    // Fix negative zero
    return (rounded == 0.0) ? 0.0 : rounded;
}

// Print results
static void print_results(StationMap* map) {
    // Create array of pointers for sorting
    Station** sorted = malloc(map->count * sizeof(Station*));
    for (int i = 0; i < map->count; i++) {
        sorted[i] = &map->stations[i];
    }

    qsort(sorted, map->count, sizeof(Station*), compare_stations);

    printf("{");
    for (int i = 0; i < map->count; i++) {
        Station* s = sorted[i];
        double min = round_temp(s->min / 10.0);
        double mean = round_temp((s->sum / 10.0) / s->count);
        double max = round_temp(s->max / 10.0);

        if (i > 0) printf(", ");
        printf("%s=%.1f/%.1f/%.1f", s->name, min, mean, max);
    }
    printf("}\n");

    free(sorted);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <measurements_file>\n", argv[0]);
        return 1;
    }

    // Open file
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }

    size_t file_size = sb.st_size;

    // Memory map file
    char* data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // Advise kernel about access pattern
    madvise(data, file_size, MADV_SEQUENTIAL | MADV_WILLNEED);

    // Initialize map
    StationMap* map = calloc(1, sizeof(StationMap));

    // Process data
    process_chunk(map, data, file_size);

    // Print results
    print_results(map);

    // Cleanup
    munmap(data, file_size);
    close(fd);
    free(map);

    return 0;
}
