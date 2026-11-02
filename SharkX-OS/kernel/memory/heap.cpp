#include <memory/heap.h>
#include <kernel/io.h>
#include <lib/string.h>
#include <lib/printf.h>

namespace
{
constexpr size_t HEAP_SIZE = 8 * 1024 * 1024;
constexpr uint32_t HEAP_MAGIC = 0x53484B58;
constexpr uint32_t BLOCK_FREE = 0x01;

struct BlockHeader
{
    uint32_t magic;
    uint32_t flags;
    size_t size;
    BlockHeader* next;
    BlockHeader* previous;
};

alignas(16) uint8_t g_heap_storage[HEAP_SIZE];
BlockHeader* g_first_block = nullptr;
size_t g_heap_size = 0;
uint64_t g_allocations = 0;
uint64_t g_frees = 0;
uint64_t g_failures = 0;
uint64_t g_double_frees = 0;

size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

bool valid_header(BlockHeader* header)
{
    return header != nullptr &&
           (uint8_t*)header >= g_heap_storage &&
           (uint8_t*)header < g_heap_storage + g_heap_size &&
           header->magic == HEAP_MAGIC;
}

void split_block(BlockHeader* block, size_t wanted)
{
    if (block->size < wanted + sizeof(BlockHeader) + 16)
    {
        return;
    }

    BlockHeader* rest = (BlockHeader*)((uint8_t*)(block + 1) + wanted);
    rest->magic = HEAP_MAGIC;
    rest->flags = BLOCK_FREE;
    rest->size = block->size - wanted - sizeof(BlockHeader);
    rest->next = block->next;
    rest->previous = block;

    if (block->next != nullptr)
    {
        block->next->previous = rest;
    }

    block->next = rest;
    block->size = wanted;
}

void merge_with_next(BlockHeader* block)
{
    BlockHeader* next = block->next;
    if (next == nullptr || !valid_header(next) || (next->flags & BLOCK_FREE) == 0)
    {
        return;
    }

    block->size += sizeof(BlockHeader) + next->size;
    block->next = next->next;
    if (block->next != nullptr)
    {
        block->next->previous = block;
    }
}

void merge_with_previous(BlockHeader* block)
{
    BlockHeader* previous = block->previous;
    if (previous == nullptr || !valid_header(previous) || (previous->flags & BLOCK_FREE) == 0)
    {
        return;
    }

    previous->size += sizeof(BlockHeader) + block->size;
    previous->next = block->next;
    if (previous->next != nullptr)
    {
        previous->next->previous = previous;
    }
}

void initialize_block(BlockHeader* block, size_t size)
{
    block->magic = HEAP_MAGIC;
    block->flags = BLOCK_FREE;
    block->size = size;
    block->next = nullptr;
    block->previous = nullptr;
}
}

void heap_init(void)
{
    g_heap_size = HEAP_SIZE;
    g_first_block = (BlockHeader*)g_heap_storage;
    initialize_block(g_first_block, g_heap_size - sizeof(BlockHeader));
    g_allocations = 0;
    g_frees = 0;
    g_failures = 0;
    g_double_frees = 0;
}

void* kmalloc(size_t size)
{
    if (size == 0 || size > g_heap_size)
    {
        ++g_failures;
        return nullptr;
    }

    size_t wanted = align_up(size, 16);
    if (wanted < size)
    {
        ++g_failures;
        return nullptr;
    }

    uint64_t flags = irq_save();

    for (BlockHeader* block = g_first_block; block != nullptr; block = block->next)
    {
        if (!valid_header(block))
        {
            break;
        }

        if ((block->flags & BLOCK_FREE) != 0 && block->size >= wanted)
        {
            split_block(block, wanted);
            block->flags &= ~BLOCK_FREE;
            ++g_allocations;
            irq_restore(flags);
            return (void*)(block + 1);
        }
    }

    ++g_failures;
    irq_restore(flags);
    return nullptr;
}

void* kzalloc(size_t size)
{
    void* pointer = kmalloc(size);
    if (pointer != nullptr)
    {
        memset(pointer, 0, size);
    }
    return pointer;
}

void kfree(void* pointer)
{
    if (pointer == nullptr)
    {
        return;
    }

    uint64_t flags = irq_save();

    BlockHeader* block = (BlockHeader*)pointer - 1;
    if (!valid_header(block))
    {
        irq_restore(flags);
        return;
    }

    if ((block->flags & BLOCK_FREE) != 0)
    {
        ++g_double_frees;
        irq_restore(flags);
        return;
    }

    block->flags |= BLOCK_FREE;
    ++g_frees;

    merge_with_next(block);
    merge_with_previous(block);

    irq_restore(flags);
}

void* krealloc(void* pointer, size_t size)
{
    if (pointer == nullptr)
    {
        return kmalloc(size);
    }

    if (size == 0)
    {
        kfree(pointer);
        return nullptr;
    }

    uint64_t flags = irq_save();
    BlockHeader* block = (BlockHeader*)pointer - 1;
    bool valid = valid_header(block) && (block->flags & BLOCK_FREE) == 0;
    size_t old_size = valid ? block->size : 0;
    irq_restore(flags);

    if (!valid)
    {
        return nullptr;
    }

    if (old_size >= size)
    {
        return pointer;
    }

    void* replacement = kmalloc(size);
    if (replacement == nullptr)
    {
        return nullptr;
    }

    memcpy(replacement, pointer, old_size);
    kfree(pointer);
    return replacement;
}

bool heap_owns(const void* pointer)
{
    const uint8_t* p = (const uint8_t*)pointer;
    return p >= g_heap_storage && p < g_heap_storage + g_heap_size;
}

HeapStats heap_stats(void)
{
    HeapStats stats = {};
    stats.total_bytes = g_heap_size;

    uint64_t flags = irq_save();

    for (BlockHeader* block = g_first_block; block != nullptr; block = block->next)
    {
        if (!valid_header(block))
        {
            break;
        }

        ++stats.block_count;

        if ((block->flags & BLOCK_FREE) != 0)
        {
            ++stats.free_block_count;
            if (block->size > stats.largest_free_block)
            {
                stats.largest_free_block = block->size;
            }
        }
        else
        {
            stats.used_bytes += sizeof(BlockHeader) + block->size;
        }
    }

    stats.allocations = g_allocations;
    stats.frees = g_frees;
    stats.failures = g_failures;
    stats.double_free_detected = g_double_frees;

    irq_restore(flags);

    if (stats.used_bytes > stats.total_bytes)
    {
        stats.used_bytes = stats.total_bytes;
    }
    stats.free_bytes = stats.total_bytes - stats.used_bytes;
    return stats;
}

void heap_selftest(void)
{
    void* a = kmalloc(64);
    void* b = kmalloc(1024);
    void* c = kmalloc(4096);

    kprintf("[heap] allocations: a=%p b=%p c=%p\n", a, b, c);
    kprintf("[heap] free a, double-free a, free b, free c\n");
    kfree(a);
    kfree(a);
    kfree(b);
    kfree(c);

    void* huge = kmalloc(g_heap_size * 2);
    kprintf("[heap] out-of-memory request returned %p (expected 0)\n", huge);

    HeapStats stats = heap_stats();
    kprintf("[heap] total=%zu used=%zu free=%zu largest_free=%zu\n",
            stats.total_bytes, stats.used_bytes, stats.free_bytes, stats.largest_free_block);
    kprintf("[heap] allocs=%llu frees=%llu failures=%llu double_frees=%llu\n",
            (unsigned long long)stats.allocations,
            (unsigned long long)stats.frees,
            (unsigned long long)stats.failures,
            (unsigned long long)stats.double_free_detected);
}

void* operator new(size_t size)
{
    return kmalloc(size);
}

void* operator new[](size_t size)
{
    return kmalloc(size);
}

void operator delete(void* pointer) noexcept
{
    kfree(pointer);
}

void operator delete[](void* pointer) noexcept
{
    kfree(pointer);
}

void operator delete(void* pointer, size_t) noexcept
{
    kfree(pointer);
}

void operator delete[](void* pointer, size_t) noexcept
{
    kfree(pointer);
}
