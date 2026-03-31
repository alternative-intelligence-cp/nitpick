/*
 * bench_helpers.c — Timing utilities and library-level stack for benchmarking
 *
 * This provides:
 *   1. High-resolution timing via clock_gettime(CLOCK_MONOTONIC)
 *   2. Peak RSS measurement via getrusage()
 *   3. A "third-party library" stack — what a package author would build
 *      without any compiler builtin support, using malloc + manual type tags.
 *
 * Compile: gcc -O2 -c bench_helpers.c -o bench_helpers.o
 *          ar rcs libbench.a bench_helpers.o
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

/* ================================================================
 * Timing
 * ================================================================ */

int64_t bench_nanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

/* ================================================================
 * Memory measurement
 * ================================================================ */

int64_t bench_peak_rss_kb(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (int64_t)usage.ru_maxrss;  /* KB on Linux */
}

/* ================================================================
 * Library-level stack ("third-party package" simulation)
 *
 * This is what a skilled library author would write in C, exposed
 * to Aria via extern. It mirrors the builtin user stack semantics:
 * - 16 bytes per slot (8 value + 8 tag)
 * - Fatal on overflow/underflow/type mismatch
 * - Same type tag scheme: int8=0, int16=1, int32=2, int64=3,
 *   flt32=4, flt64=5, bool=6, string=7, pointer=8
 *
 * Differences from the builtin:
 * - Uses malloc (not mmap)
 * - Handle is a pointer cast to int64, not an index into a table
 * - No compiler integration (caller must specify tags manually)
 * ================================================================ */

typedef struct {
    int64_t *values;
    int64_t *tags;
    int64_t  capacity;
    int64_t  top;
} LibStack;

int64_t libstack_new(int64_t capacity) {
    LibStack *s = (LibStack *)malloc(sizeof(LibStack));
    if (!s) {
        fprintf(stderr, "[libstack] allocation failed\n");
        exit(1);
    }
    s->values   = (int64_t *)malloc((size_t)capacity * sizeof(int64_t));
    s->tags     = (int64_t *)malloc((size_t)capacity * sizeof(int64_t));
    if (!s->values || !s->tags) {
        fprintf(stderr, "[libstack] allocation failed\n");
        exit(1);
    }
    s->capacity = capacity;
    s->top      = 0;
    return (int64_t)(uintptr_t)s;
}

int64_t libstack_push(int64_t handle, int64_t value, int64_t tag) {
    LibStack *s = (LibStack *)(uintptr_t)handle;
    if (s->top >= s->capacity) {
        fprintf(stderr, "[libstack] overflow: stack full (%ld/%ld)\n",
                (long)s->top, (long)s->capacity);
        exit(1);
    }
    s->values[s->top] = value;
    s->tags[s->top]   = tag;
    s->top++;
    return 0;
}

int64_t libstack_pop(int64_t handle, int64_t expected_tag) {
    LibStack *s = (LibStack *)(uintptr_t)handle;
    if (s->top <= 0) {
        fprintf(stderr, "[libstack] underflow: stack empty\n");
        exit(1);
    }
    s->top--;
    if (expected_tag >= 0 && s->tags[s->top] != expected_tag) {
        fprintf(stderr, "[libstack] type mismatch: expected tag %ld, got %ld\n",
                (long)expected_tag, (long)s->tags[s->top]);
        exit(1);
    }
    return s->values[s->top];
}

int64_t libstack_peek(int64_t handle, int64_t expected_tag) {
    LibStack *s = (LibStack *)(uintptr_t)handle;
    if (s->top <= 0) {
        fprintf(stderr, "[libstack] underflow on peek: stack empty\n");
        exit(1);
    }
    int64_t idx = s->top - 1;
    if (expected_tag >= 0 && s->tags[idx] != expected_tag) {
        fprintf(stderr, "[libstack] type mismatch on peek: expected %ld, got %ld\n",
                (long)expected_tag, (long)s->tags[idx]);
        exit(1);
    }
    return s->values[idx];
}

int64_t libstack_destroy(int64_t handle) {
    LibStack *s = (LibStack *)(uintptr_t)handle;
    if (s) {
        free(s->values);
        free(s->tags);
        free(s);
    }
    return 0;
}
