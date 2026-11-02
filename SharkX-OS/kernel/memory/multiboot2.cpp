#include <kernel/multiboot2.h>
#include <lib/string.h>

namespace
{
uint64_t align_up_8(uint64_t value)
{
    return (value + 7ULL) & ~7ULL;
}

struct SummaryContext
{
    uint64_t total_bytes;
    uint64_t available_bytes;
};
}

void multiboot_iterate(uint64_t mbi_addr, void (*callback)(const MultibootTag*, void*), void* context)
{
    if (mbi_addr == 0 || callback == nullptr)
    {
        return;
    }

    const MultibootInfo* info = (const MultibootInfo*)mbi_addr;
    if (info->total_size < sizeof(MultibootInfo))
    {
        return;
    }

    uint8_t* current = (uint8_t*)mbi_addr + sizeof(MultibootInfo);
    uint8_t* end = (uint8_t*)mbi_addr + info->total_size;

    while (current + sizeof(MultibootTag) <= end)
    {
        const MultibootTag* tag = (const MultibootTag*)current;
        if (tag->type == MULTIBOOT2_TAG_END)
        {
            break;
        }
        if (tag->size < sizeof(MultibootTag))
        {
            break;
        }

        callback(tag, context);

        uint64_t next = align_up_8((uint64_t)current + tag->size);
        if (next <= (uint64_t)current)
        {
            break;
        }
        current = (uint8_t*)next;
    }
}

const MultibootTag* multiboot_find_tag(uint64_t mbi_addr, uint32_t type)
{
    if (mbi_addr == 0)
    {
        return nullptr;
    }

    const MultibootInfo* info = (const MultibootInfo*)mbi_addr;
    if (info->total_size < sizeof(MultibootInfo))
    {
        return nullptr;
    }

    uint8_t* current = (uint8_t*)mbi_addr + sizeof(MultibootInfo);
    uint8_t* end = (uint8_t*)mbi_addr + info->total_size;

    while (current + sizeof(MultibootTag) <= end)
    {
        const MultibootTag* tag = (const MultibootTag*)current;
        if (tag->type == MULTIBOOT2_TAG_END)
        {
            break;
        }
        if (tag->size < sizeof(MultibootTag))
        {
            break;
        }
        if (tag->type == type)
        {
            return tag;
        }

        uint64_t next = align_up_8((uint64_t)current + tag->size);
        if (next <= (uint64_t)current)
        {
            break;
        }
        current = (uint8_t*)next;
    }

    return nullptr;
}

const char* multiboot_cmdline(uint64_t mbi_addr)
{
    const MultibootTag* tag = multiboot_find_tag(mbi_addr, MULTIBOOT2_TAG_CMDLINE);
    if (tag == nullptr)
    {
        return "";
    }
    const MultibootCmdlineTag* cmdline = (const MultibootCmdlineTag*)tag;
    return cmdline->string;
}

namespace
{
void summary_callback(const MultibootTag* tag, void* context)
{
    SummaryContext* summary = (SummaryContext*)context;

    if (tag->type == MULTIBOOT2_TAG_MMAP)
    {
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
                if (entry_end > summary->total_bytes)
                {
                    summary->total_bytes = entry_end;
                }
                summary->available_bytes += entry->length;
            }
            cursor += mmap->entry_size;
        }
    }
}
}

bool multiboot_memory_summary(uint64_t mbi_addr, uint64_t* total_bytes, uint64_t* available_bytes)
{
    SummaryContext summary = { 0, 0 };
    multiboot_iterate(mbi_addr, summary_callback, &summary);

    if (total_bytes != nullptr)
    {
        *total_bytes = summary.total_bytes;
    }
    if (available_bytes != nullptr)
    {
        *available_bytes = summary.available_bytes;
    }

    return summary.available_bytes != 0;
}
