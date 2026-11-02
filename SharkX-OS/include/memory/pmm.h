#pragma once

#include <stdint.h>
#include <stddef.h>

#define PMM_MAX_MANAGED_BYTES (1024ULL * 1024ULL * 1024ULL)
#define PMM_MAX_FRAMES (PMM_MAX_MANAGED_BYTES / 4096ULL)

struct PmmStats
{
    uint64_t multiboot_total_bytes;
    uint64_t multiboot_available_bytes;
    uint64_t total_frames;
    uint64_t free_frames;
    uint64_t used_frames;
    uint64_t reserved_frames;
    uint64_t kernel_start;
    uint64_t kernel_end;
};

void pmm_init(uint64_t mbi_addr);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* frame);
bool pmm_frame_is_free(uint64_t frame_index);
const PmmStats& pmm_stats(void);
