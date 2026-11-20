#define _DEFAULT_SOURCE
#include "myalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define OPERATIONS 50000U
#define SLOTS 512U
#define SEED 20261120U

typedef struct {
    unsigned short slot;
    unsigned char allocate;
    unsigned short size;
} operation_t;

typedef struct {
    double seconds;
    size_t heap_bytes;
    size_t free_bytes;
    size_t free_blocks;
    size_t largest_free;
    double fragmentation;
    int custom;
} result_t;

static operation_t trace_ops[OPERATIONS];

static uint32_t next_random(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void make_trace(void)
{
    unsigned char active[SLOTS] = {0};
    uint32_t state = SEED;
    size_t i;
    for (i = 0; i < OPERATIONS; ++i) {
        unsigned slot = next_random(&state) % SLOTS;
        trace_ops[i].slot = (unsigned short)slot;
        trace_ops[i].allocate = (unsigned char)!active[slot];
        if (trace_ops[i].allocate) {
            trace_ops[i].size = (unsigned short)(1U + next_random(&state) % 4096U);
            active[slot] = 1U;
        } else {
            trace_ops[i].size = 0U;
            active[slot] = 0U;
        }
    }
}

static int write_result(int fd, const result_t *r)
{
    const char *p = (const char *)r;
    size_t left = sizeof(*r);
    while (left != 0U) {
        ssize_t n = write(fd, p, left);
        if (n <= 0) return 0;
        p += (size_t)n;
        left -= (size_t)n;
    }
    return 1;
}

static void run_child(int kind, int fd)
{
    void *slots[SLOTS] = {0};
    result_t result;
    struct timespec start, finish;
    size_t i;
    memset(&result, 0, sizeof(result));
    result.custom = kind != 2;
    if (kind == 0) set_policy(FIT_FIRST);
    if (kind == 1) set_policy(FIT_BEST);
    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < OPERATIONS; ++i) {
        operation_t op = trace_ops[i];
        if (op.allocate) {
            slots[op.slot] = kind == 2 ? malloc(op.size) : my_malloc(op.size);
        } else if (slots[op.slot] != NULL) {
            if (kind == 2) free(slots[op.slot]);
            else my_free(slots[op.slot]);
            slots[op.slot] = NULL;
        }
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &finish);
    result.seconds = (double)(finish.tv_sec - start.tv_sec) +
                     (double)(finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    if (kind != 2) {
        heap_stats_t stats;
        heap_stats(&stats);
        result.heap_bytes = stats.heap_bytes;
        result.free_bytes = stats.free_bytes;
        result.free_blocks = stats.free_blocks;
        result.largest_free = stats.largest_free;
        result.fragmentation = stats.free_bytes == 0U ? 0.0 :
            1.0 - (double)stats.largest_free / (double)stats.free_bytes;
    }
    for (i = 0; i < SLOTS; ++i) {
        if (slots[i] != NULL) {
            if (kind == 2) free(slots[i]);
            else my_free(slots[i]);
        }
    }
    (void)write_result(fd, &result);
    close(fd);
    _exit(0);
}

static int execute_case(int kind, result_t *out)
{
    int fds[2];
    pid_t child;
    int status;
    char *p = (char *)out;
    size_t left = sizeof(*out);
    if (pipe(fds) != 0) return 0;
    child = fork();
    if (child < 0) { close(fds[0]); close(fds[1]); return 0; }
    if (child == 0) { close(fds[0]); run_child(kind, fds[1]); }
    close(fds[1]);
    while (left != 0U) {
        ssize_t n = read(fds[0], p, left);
        if (n <= 0) break;
        p += (size_t)n;
        left -= (size_t)n;
    }
    close(fds[0]);
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return 0;
    return left == 0U;
}

static void print_result(const char *name, const result_t *r)
{
    printf("%-18s %9.6f s", name, r->seconds);
    if (r->custom) {
        printf("  heap=%zu free=%zu free_blocks=%zu largest=%zu fragmentation=%.4f",
               r->heap_bytes, r->free_bytes, r->free_blocks,
               r->largest_free, r->fragmentation);
    } else {
        printf("  heap statistics=N/A (system allocator)");
    }
    putchar('\n');
}

int main(void)
{
    result_t first, best, system_alloc;
    make_trace();
    if (!execute_case(0, &first) || !execute_case(1, &best) ||
        !execute_case(2, &system_alloc)) {
        fputs("Benchmark child process failed.\n", stderr);
        return 1;
    }
    printf("Trace: %u operations, %u slots, seed=%u\n", OPERATIONS, SLOTS, SEED);
    print_result("Custom first-fit", &first);
    print_result("Custom best-fit", &best);
    print_result("System malloc/free", &system_alloc);
    return 0;
}
