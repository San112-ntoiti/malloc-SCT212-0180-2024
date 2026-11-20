#define _DEFAULT_SOURCE
#include "myalloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define ALIGNMENT ((size_t)16)
#define MIN_GROW ((size_t)65536)
#define MAGIC_USED UINT64_C(0xA110CA7E5AFE1234)
#define MAGIC_FREE UINT64_C(0xF4EECA7E5AFE1234)

typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    uint64_t magic;
    int is_free;
} block_t;

#define HEADER_SIZE (((sizeof(block_t) + ALIGNMENT - 1U) / ALIGNMENT) * ALIGNMENT)

static block_t *g_head;
static block_t *g_tail;
static fit_policy_t g_policy = FIT_FIRST;
static size_t g_heap_bytes;

static int add_overflow_size(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) return 1;
    *out = a + b;
    return 0;
}

static int align_size(size_t n, size_t *out)
{
    size_t t;
    if (add_overflow_size(n, ALIGNMENT - 1U, &t)) return 0;
    *out = t & ~(ALIGNMENT - 1U);
    return 1;
}

static void *payload(block_t *b) { return (void *)((char *)b + HEADER_SIZE); }
static const void *const_payload(const block_t *b) { return (const void *)((const char *)b + HEADER_SIZE); }
static int adjacent(const block_t *a, const block_t *b)
{
    return (const char *)a + HEADER_SIZE + a->size == (const char *)b;
}

static void write_all(int fd, const char *buf, size_t len)
{
    while (len != 0U) {
        ssize_t n = write(fd, buf, len);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return;

        buf += (size_t)n;
        len -= (size_t)n;
    }
}

static void diagnostic(const char *message)
{
    write_all(STDERR_FILENO, message, strlen(message));
    write_all(STDERR_FILENO, "\n", 1U);
}

static block_t *find_pointer(const void *ptr)
{
    block_t *b;
    for (b = g_head; b != NULL; b = b->next) {
        if (payload(b) == ptr) return b;
    }
    return NULL;
}

static block_t *find_fit(size_t need)
{
    block_t *b;
    block_t *best = NULL;
    for (b = g_head; b != NULL; b = b->next) {
        if (b->is_free && b->magic == MAGIC_FREE && b->size >= need) {
            if (g_policy == FIT_FIRST) return b;
            if (best == NULL || b->size < best->size) best = b;
        }
    }
    return best;
}

static void split_block(block_t *b, size_t need)
{
    size_t remainder;
    block_t *r;
    if (b->size < need || b->size - need < HEADER_SIZE + ALIGNMENT) return;
    remainder = b->size - need - HEADER_SIZE;
    r = (block_t *)((char *)payload(b) + need);
    r->size = remainder;
    r->next = b->next;
    r->prev = b;
    r->magic = MAGIC_FREE;
    r->is_free = 1;
    if (r->next != NULL) r->next->prev = r;
    else g_tail = r;
    b->next = r;
    b->size = need;
}

static int grow_heap(size_t need)
{
    size_t request;
    size_t extra;
    uintptr_t raw;
    size_t padding;
    void *region;
    block_t *b;

    if (add_overflow_size(need, HEADER_SIZE + ALIGNMENT - 1U, &extra)) return 0;
    request = extra > MIN_GROW ? extra : MIN_GROW;
    if (!align_size(request, &request) || request > (size_t)INTPTR_MAX) return 0;
    if (g_heap_bytes > SIZE_MAX - request) return 0;

    region = sbrk(0);
    if (region == (void *)-1) return 0;
    raw = (uintptr_t)region;
    padding = (size_t)((ALIGNMENT - (raw & (ALIGNMENT - 1U))) & (ALIGNMENT - 1U));
    region = sbrk((intptr_t)request);
    if (region == (void *)-1) return 0;

    b = (block_t *)((char *)region + padding);
    b->size = (request - padding - HEADER_SIZE) & ~(ALIGNMENT - 1U);
    if (b->size < need) return 0;
    b->next = NULL;
    b->prev = g_tail;
    b->magic = MAGIC_FREE;
    b->is_free = 1;
    if (g_tail != NULL) g_tail->next = b;
    else g_head = b;
    g_tail = b;
    g_heap_bytes += request;
    return 1;
}

static void coalesce(void)
{
    block_t *b = g_head;
    while (b != NULL && b->next != NULL) {
        block_t *n = b->next;
        if (b->is_free && n->is_free && adjacent(b, n)) {
            b->size += HEADER_SIZE + n->size;
            b->next = n->next;
            if (b->next != NULL) b->next->prev = b;
            else g_tail = b;
            b->magic = MAGIC_FREE;
        } else {
            b = b->next;
        }
    }
}

void *my_malloc(size_t size)
{
    size_t need;
    block_t *b;
    if (size == 0U) return NULL;
    if (!align_size(size, &need)) return NULL;
    b = find_fit(need);
    if (b == NULL) {
        if (!grow_heap(need)) return NULL;
        b = find_fit(need);
        if (b == NULL) return NULL;
    }
    split_block(b, need);
    b->is_free = 0;
    b->magic = MAGIC_USED;
    return payload(b);
}

void my_free(void *ptr)
{
    block_t *b;
    if (ptr == NULL) return;
    b = find_pointer(ptr);
    if (b == NULL) {
        diagnostic("my_free: invalid pointer");
        return;
    }
    if (b->is_free && b->magic == MAGIC_FREE) {
        diagnostic("my_free: double free detected");
        return;
    }
    if (b->magic != MAGIC_USED || b->is_free) {
        diagnostic("my_free: invalid pointer");
        return;
    }
    b->is_free = 1;
    b->magic = MAGIC_FREE;
    coalesce();
}

void *my_calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *p;
    if (nmemb == 0U || size == 0U) return NULL;
    if (nmemb > SIZE_MAX / size) return NULL;
    total = nmemb * size;
    p = my_malloc(total);
    if (p != NULL) memset(p, 0, total);
    return p;
}

void *my_realloc(void *ptr, size_t size)
{
    block_t *b;
    size_t need;
    size_t old_size;
    void *replacement;
    if (ptr == NULL) return my_malloc(size);
    if (size == 0U) { my_free(ptr); return NULL; }
    b = find_pointer(ptr);
    if (b == NULL || b->magic != MAGIC_USED || b->is_free) return NULL;
    if (!align_size(size, &need)) return NULL;
    old_size = b->size;
    if (need <= old_size) {
        split_block(b, need);
        coalesce();
        return ptr;
    }
    while (b->next != NULL && b->next->is_free && adjacent(b, b->next) && b->size < need) {
        block_t *n = b->next;
        b->size += HEADER_SIZE + n->size;
        b->next = n->next;
        if (b->next != NULL) b->next->prev = b;
        else g_tail = b;
    }
    if (b->size >= need) {
        split_block(b, need);
        return ptr;
    }
    replacement = my_malloc(size);
    if (replacement == NULL) return NULL;
    memcpy(replacement, ptr, old_size < size ? old_size : size);
    my_free(ptr);
    return replacement;
}

void heap_stats(heap_stats_t *out)
{
    block_t *b;
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    out->heap_bytes = g_heap_bytes;
    for (b = g_head; b != NULL; b = b->next) {
        out->blocks++;
        if (b->is_free) {
            out->free_blocks++;
            out->free_bytes += b->size;
            if (b->size > out->largest_free) out->largest_free = b->size;
        } else {
            out->used_bytes += b->size;
        }
    }
}

void heap_dump(void)
{
    block_t *b;
    size_t i = 0U;
    char line[160];

    for (b = g_head; b != NULL; b = b->next, ++i) {
        int n = snprintf(
            line,
            sizeof(line),
            "%zu: %p size=%zu %s\n",
            i,
            (void *)b,
            b->size,
            b->is_free ? "FREE" : "USED"
        );

        if (n > 0) {
            size_t length = (size_t)n;

            if (length >= sizeof(line))
                length = sizeof(line) - 1U;

            write_all(STDOUT_FILENO, line, length);
        }
    }
}

void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST) g_policy = p;
}

int heap_check(void)
{
    block_t *b;
    block_t *previous = NULL;
    size_t count = 0U;
    size_t limit = g_heap_bytes / (HEADER_SIZE + ALIGNMENT) + 2U;
    if ((g_head == NULL) != (g_tail == NULL)) return 0;
    for (b = g_head; b != NULL; b = b->next) {
        if (++count > limit) return 0;
        if (b->prev != previous) return 0;
        if (b->size == 0U || (b->size & (ALIGNMENT - 1U)) != 0U) return 0;
        if (((uintptr_t)const_payload(b) & (ALIGNMENT - 1U)) != 0U) return 0;
        if (b->is_free) { if (b->magic != MAGIC_FREE) return 0; }
        else if (b->magic != MAGIC_USED) return 0;
        if (previous != NULL) {
            if ((uintptr_t)b <= (uintptr_t)previous) return 0;
            if ((char *)previous + HEADER_SIZE + previous->size > (char *)b) return 0;
            if (previous->is_free && b->is_free && adjacent(previous, b)) return 0;
        }
        previous = b;
    }
    return previous == g_tail;
}
