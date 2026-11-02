#include <memory/pmm.h>
#include <kernel/multiboot2.h>
#include <kernel/io.h>
#include <lib/string.h>

namespace
{
constexpr uint64_t FRAME_SIZE = 4096ULL;
constexpr uint64_t BITMAP_BYTES = PMM_MAX_FRAMES / 8;

uint8_t g_bitmap[BITMAP_BYTES];
PmmStats g_stats;

bool bitmap_test(uint64_t frame)
{
    return (g_bitmap[frame / 8] & (uint8_t)(1u << (frame % 8))) != 0;
}

void bitmap_set(uint64_t frame, bool used)
{
    uint8_t mask = (uint8_t)(1u << (frame % 8));
    if (used)
    {
        g_bitmap[frame / 8] |= mask;
    }
    else
    {
        g_bitmap[frame / 8] &= (uint8_t)~mask;
    }
}

void mark_used(uint64_t frame)
{
    if (frame < g_stats.total_frames && !bitmap_test(frame))
    {
        bitmap_set(frame, true);
        ++g_stats.used_frames;
        if (g_stats.free_frames > 0)
        {
            --g_stats.free_frames;
        }
    }
}

void mark_free(uint64_t frame)
{
    if (frame < g_stats.total_frames && bitmap_test(frame))
    {
        bitmap_set(frame, false);
        ++g_stats.free_frames;
        if (g_stats.used_frames > 0)
        {
            --g_stats.used_frames;
        }
    }
}

void reserve_range(uint64_t start, uint64_t end)
{
    if (end <= start)
    {
        return;
    }

    uint64_t first = start / FRAME_SIZE;
    uint64_t last = (end + FRAME_SIZE - 1) / FRAME_SIZE;

    if (last > g_stats.total_frames)
    {
        last = g_stats.total_frames;
    }

    for (uint64_t frame = first; frame < last; ++frame)
    {
        mark_used(frame);
    }
}

struct PmmScanContext
{
    uint64_t max_end;
    uint64_t available_bytes;
};

void pmm_scan_callback(const MultibootTag* tag, void* context)
{
    PmmScanContext* scan = (PmmScanContext*)context;

    if (tag->type != MULTIBOOT2_TAG_MMAP)
    {
        return;
    }

    const MultibootMmapTag* mmap = (const MultibootMmapTag*)tag;
    if (mmap->entry_size < sizeof(MultibootMmapEntry))
    {
        return;
    }

    uint8_t* cursor = (uint8_t*)mmap + 16;
    uint8_t* end = (uint8_t*)mmap + mmap->size;

    while (cursor + sizeof(MultibootMmapEntry) <= end)
    {
        const MultibootMmapEntry* entry = (const MultibootMmapEntry*)cursor;
        uint64_t entry_end = entry->base_addr + entry->length;
        if (entry->type == 1)
        {
            if (entry_end > scan->max_end)
            {
                scan->max_end = entry_end;
            }
            if (entry->length >= FRAME_SIZE)
            {
                scan->available_bytes += entry->length;
            }
        }
        cursor += mmap->entry_size;
    }
}

void free_available_callback(const MultibootTag* tag, void* context)
{
    (void)context;

    if (tag->type != MULTIBOOT2_TAG_MMAP)
    {
        return;
    }

    const MultibootMmapTag* mmap = (const MultibootMmapTag*)tag;
    if (mmap->entry_size < sizeof(MultibootMmapEntry))
    {
        return;
    }

    uint8_t* cursor = (uint8_t*)mmap + 16;
    uint8_t* end = (uint8_t*)mmap + mmap->size;

    while (cursor + sizeof(MultibootMmapEntry) <= end)
    {
        const MultibootMmapEntry* entry = (const MultibootMmapEntry*)cursor;

        if (entry->type == 1)
        {
            uint64_t start = entry->base_addr;
            uint64_t finish = start + entry->length;

            if (finish > PMM_MAX_MANAGED_BYTES)
            {
                finish = PMM_MAX_MANAGED_BYTES;
            }

            uint64_t first = (start + FRAME_SIZE - 1) / FRAME_SIZE;
            uint64_t last = finish / FRAME_SIZE;

            for (uint64_t frame = first; frame < last; ++frame)
            {
                mark_free(frame);
            }
        }

        cursor += mmap->entry_size;
    }
}
}

extern "C" char __kernel_start[];
extern "C" char __kernel_end[];

void pmm_init(uint64_t mbi_addr)
{
    memset(&g_stats, 0, sizeof(g_stats));
    memset(g_bitmap, 0xFF, sizeof(g_bitmap));

    g_stats.kernel_start = (uint64_t)__kernel_start;
    g_stats.kernel_end = (uint64_t)__kernel_end;

    PmmScanContext scan = { 0, 0 };
    multiboot_iterate(mbi_addr, pmm_scan_callback, &scan);

    g_stats.multiboot_total_bytes = scan.max_end;
    g_stats.multiboot_available_bytes = scan.available_bytes;

    uint64_t highest = scan.max_end;
    if (highest < g_stats.kernel_end)
    {
        highest = g_stats.kernel_end;
    }
    if (highest == 0)
    {
        highest = 64ULL * 1024ULL * 1024ULL;
    }
    if (highest > PMM_MAX_MANAGED_BYTES)
    {
        highest = PMM_MAX_MANAGED_BYTES;
    }

    g_stats.total_frames = highest / FRAME_SIZE;
    if (g_stats.total_frames > PMM_MAX_FRAMES)
    {
        g_stats.total_frames = PMM_MAX_FRAMES;
    }
    g_stats.used_frames = g_stats.total_frames;
    g_stats.free_frames = 0;

    multiboot_iterate(mbi_addr, free_available_callback, nullptr);

    reserve_range(0, g_stats.kernel_end);

    if (mbi_addr != 0)
    {
        const MultibootInfo* info = (const MultibootInfo*)mbi_addr;
        uint64_t mbi_end = mbi_addr + info->total_size;
        reserve_range(mbi_addr, mbi_end + FRAME_SIZE);
    }

    g_stats.reserved_frames = g_stats.used_frames;
}

void* pmm_alloc_frame(void)
{
    uint64_t flags = irq_save();
    for (uint64_t frame = 0; frame < g_stats.total_frames; ++frame)
    {
        if (!bitmap_test(frame))
        {
            mark_used(frame);
            irq_restore(flags);
            return (void*)(frame * FRAME_SIZE);
        }
    }
    irq_restore(flags);
    return nullptr;
}

void pmm_free_frame(void* frame)
{
    uint64_t address = (uint64_t)frame;
    if ((address % FRAME_SIZE) != 0)
    {
        return;
    }

    uint64_t index = address / FRAME_SIZE;
    if (index >= g_stats.total_frames)
    {
        return;
    }

    uint64_t flags = irq_save();
    mark_free(index);
    irq_restore(flags);
}

bool pmm_frame_is_free(uint64_t frame_index)
{
    if (frame_index >= g_stats.total_frames)
    {
        return false;
    }
    return !bitmap_test(frame_index);
}

const PmmStats& pmm_stats(void)
{
    return g_stats;
}
