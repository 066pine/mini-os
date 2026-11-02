#pragma once

#include <stdint.h>
#include <stddef.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289u
#define MULTIBOOT2_TAG_END 0u
#define MULTIBOOT2_TAG_CMDLINE 1u
#define MULTIBOOT2_TAG_MODULE 3u
#define MULTIBOOT2_TAG_BASIC_MEMINFO 4u
#define MULTIBOOT2_TAG_MMAP 6u
#define MULTIBOOT2_TAG_FRAMEBUFFER 8u

struct MultibootTag
{
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct MultibootInfo
{
    uint32_t total_size;
    uint32_t reserved;
    MultibootTag tags[];
} __attribute__((packed));

struct MultibootMmapEntry
{
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct MultibootMmapTag
{
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    MultibootMmapEntry entries[];
} __attribute__((packed));

struct MultibootCmdlineTag
{
    uint32_t type;
    uint32_t size;
    char string[];
} __attribute__((packed));

struct MultibootTagIterator
{
    uint8_t* current;
    uint8_t* end;
};

void multiboot_iterate(uint64_t mbi_addr, void (*callback)(const MultibootTag*, void*), void* context);
const MultibootTag* multiboot_find_tag(uint64_t mbi_addr, uint32_t type);
const char* multiboot_cmdline(uint64_t mbi_addr);
bool multiboot_memory_summary(uint64_t mbi_addr, uint64_t* total_bytes, uint64_t* available_bytes);
