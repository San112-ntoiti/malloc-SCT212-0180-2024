# Custom Memory Allocator — Question 2

**Student:** Newton Ntoiti  
**Registration:** SCT212-0180/2024  
**Course:** ICS 2305 — Systems Programming

## Build and test

```bash
make
make test
make clean
make ubsan
make bench
```

The allocator implementation is in `myalloc.c`; its public interface is in `myalloc.h`. The lecturer-provided test harness is preserved in `test_myalloc.c`. It exercises allocation, alignment, non-overlap, reuse, splitting, coalescing, calloc, realloc, large requests, placement policies, randomized stress testing and heap integrity.

## Implementation status

Update this table only after running the tests and inspecting the implementation.

| Task | Area | Status |
|---|---|---|
| 1 | Block metadata and alignment | Implemented; local regression tests pass |
| 2 | Heap growth with `sbrk()` | Implemented; local regression tests pass |
| 3 | Allocation and block splitting | Implemented; local regression tests pass |
| 4 | Free, validation and coalescing | Implemented; local regression tests pass |
| 5 | `calloc()` and `realloc()` | Implemented; local regression tests pass |
| 6 | First-fit and best-fit | Implemented; local regression tests pass |
| 7 | Statistics, dump and consistency check | Implemented; local regression tests pass |
| 8 | Benchmark and analysis | Benchmark implemented; record measurements from your Ubuntu run in `REPORT.md` |

## Design summary

Each heap block begins with allocator metadata followed by a payload. The metadata header is rounded up to a 16-byte boundary, and requested payload sizes are rounded to the same alignment. This keeps every returned payload suitably aligned. The allocator obtains memory from the process break using `sbrk()` and requests at least 64 KiB per growth operation. New regions are recorded as separate blocks; the implementation does not assume that separate `sbrk()` regions are physically contiguous.

Allocation searches the free list using either first-fit or best-fit. A sufficiently large free block is split, leaving a new free block in the list. Freeing validates that the supplied pointer exactly matches a known payload before changing metadata. Adjacent free blocks are merged only when their addresses prove physical contiguity. `calloc` checks multiplication overflow before allocation and clears the requested bytes. `realloc` handles null and zero-size requests, shrinks in place, attempts to expand into an adjacent free block, and otherwise allocates a replacement while preserving the old contents. Statistics report payload capacity separately from total bytes requested from the operating system.

## Required written questions

1. **Why align the header and payload to 16 bytes?** The header size is rounded to a multiple of 16 so adding it to an aligned block address leaves the payload aligned. Requested sizes are also rounded up, so the next block header remains aligned.
2. **Why can `printf()` inside `my_malloc()` recurse?** Formatted I/O may itself allocate memory through the C library. If that allocation invokes the custom allocator, the allocator re-enters itself while already allocating. The implementation therefore uses `write()` for diagnostics and `snprintf()` only to format heap-dump lines.
3. **Which field enables O(1) access to the previous list node?** The `prev` pointer. Together with `next`, it forms a doubly linked list.
4. **Internal versus external fragmentation?** Internal fragmentation is unused space inside an allocated block, such as alignment padding. External fragmentation is free memory split into separated holes that cannot satisfy a larger request even when their total size is sufficient.
5. **Why is memory not returned to the OS here?** This implementation only grows the break and does not shrink it. A possible extension is to lower the break with `brk()`/`sbrk()` when a free block reaches the current program break and no later live allocation depends on that region.
6. **Why might consecutive list nodes not be adjacent in memory?** Other runtime/library users can change the process break between allocator growth calls, or a growth request can be separated from an earlier region. The allocator merges nodes only after checking that the end address of one block exactly equals the next block's header address.

## Validation record

The lecturer-provided test harness completed with 29 passed and 0 failed on the student's native Ubuntu installation.

The UndefinedBehaviorSanitizer build also completed with 29 passed and 0 failed.

The tests covered basic allocation, 16-byte alignment, non-overlapping blocks, memory reuse, splitting, coalescing, calloc, realloc, heap growth through sbrk(), first-fit and best-fit policies, randomized stress testing and heap consistency.

## AI assistance declaration

ChatGPT was used to assist with implementation planning, code drafting and documentation. The student is responsible for reviewing, testing, understanding and submitting the final work.
