#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <math.h>

#define TMP_PATH "perf_test"
#define N 1000000

unsigned sum_array(size_t n, unsigned *arr) {
    unsigned x = 0;
    for (unsigned *ptr = arr, *end = arr + n; ptr != end; ++ptr) {
        x += *ptr;
    }

    return x;
}

unsigned *read_array(const char *file_path, size_t *n) {
    FILE *f = fopen(file_path, "r");
    if (!f) return NULL;
    if (fread(n, sizeof(*n), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    unsigned *res = (unsigned*)malloc(sizeof(unsigned) * *n);
    if (fread(res, sizeof(*res) * *n, 1, f) != 1) {
        fclose(f);
        free(res);
        return NULL;
    }
    fclose(f);
    return res;
}

void write_test(const char *file_path, size_t n, unsigned ini, unsigned step) {
    FILE *f = fopen(file_path, "w");
    unsigned x = ini;
    fwrite(&n, sizeof(n), 1, f);
    for (size_t i = 0; i < n; ++i, x += step) {
        fwrite(&x, sizeof(x), 1, f);
    }
    fclose(f);
}

unsigned compute_test_res(size_t n, unsigned ini, unsigned step) {
    unsigned x = ini;
    unsigned s = 0;
    for (size_t i = 0; i < n; ++i, x += step) {
        s += x;
    }

    return s;
}

void run_tests() {
    for (int i = 0; i < 10; ++i) {
        write_test(TMP_PATH, N, i * 3, 10 + i);
        size_t n;
        unsigned *arr = read_array(TMP_PATH, &n);
        if (arr == NULL) {
            printf("test %d: %s\n", i, "IO fail");
            return;
        } else {
            printf("test %d: %s\n", i, compute_test_res(N, i * 3, 10 + i) == sum_array(n, arr) ? "ok" : "fail");
            free(arr);
        }
    }
}

#define WORK_ITERATIONS 5

int main() {
    for (int itr = 0; itr < 5; ++itr) {
        run_tests();
    }
    struct timespec start, end;
    double ts[WORK_ITERATIONS];
    for (int itr = 0; itr < WORK_ITERATIONS; ++itr) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        run_tests();
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        double delta = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
        ts[itr] = delta;
    }
    remove(TMP_PATH);

    double mean = 0;
    for (int i = 0; i < WORK_ITERATIONS; ++i) {
        mean += ts[i];
    }
    mean /= WORK_ITERATIONS;
    double var = 0;
    for (int i = 0; i < WORK_ITERATIONS; ++i) {
        var += (ts[i] - mean) * (ts[i] - mean);
    }
    var /= WORK_ITERATIONS - 1;

    printf("time per iteration: ");
    fflush(stdout);
    fprintf(stderr, "%f", mean);
    printf(" +- %f s\n", sqrt(var));
}
