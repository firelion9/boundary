#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <time.h>

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
    fscanf(f, "%zu\n", n);
    unsigned *res = (unsigned*)malloc(sizeof(unsigned) * *n);
    for (unsigned *ptr = res, *end = res + *n; ptr != end; ++ptr) {
        fscanf(f, "%u ", ptr);
    }
    fclose(f);
    return res;
}

void write_test(const char *file_path, size_t n, unsigned ini, unsigned step) {
    FILE *f = fopen(file_path, "w");
    unsigned x = ini;
    fprintf(f, "%zu\n", n);
    for (size_t i = 0; i < n; ++i, x += step) {
        fprintf(f, "%u ", x);
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
        write_test("perf_test.txt", 100000, i * 3, 10 + i);
        size_t n;
        unsigned *arr = read_array("perf_test.txt", &n);
        printf("test %d: %s\n", i, compute_test_res(100000, i * 3, 10 + i) == sum_array(n, arr) ? "ok" : "fail");
        free(arr);
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
    remove("perf_test.txt");

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

    printf("time per iteration: %f +- %f s\n", mean, var);
}
