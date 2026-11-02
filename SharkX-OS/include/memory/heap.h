#pragma once

#include <stddef.h>
#include <stdint.h>

struct HeapStats
{
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t largest_free_block;
    size_t block_count;
    size_t free_block_count;
    uint64_t allocations;
    uint64_t frees;
    uint64_t failures;
    uint64_t double_free_detected;
};

void heap_init(void);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void* krealloc(void* pointer, size_t size);
void kfree(void* pointer);

bool heap_owns(const void* pointer);
HeapStats heap_stats(void);
void heap_selftest(void);

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* pointer) noexcept;
void operator delete[](void* pointer) noexcept;
void operator delete(void* pointer, size_t) noexcept;
void operator delete[](void* pointer, size_t) noexcept;
