#include <kernel/gdt.h>
#include <kernel/io.h>
#include <stdint.h>

namespace
{
struct GdtPointer
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

uint64_t g_gdt[3];
GdtPointer g_gdt_pointer;
}

void gdt_init(void)
{
    g_gdt[0] = 0x0000000000000000ULL;
    g_gdt[1] = 0x00AF9A000000FFFFULL;
    g_gdt[2] = 0x00CF92000000FFFFULL;

    g_gdt_pointer.limit = (uint16_t)(sizeof(g_gdt) - 1);
    g_gdt_pointer.base = (uint64_t)&g_gdt[0];

    asm volatile("lgdt %0" : : "m"(g_gdt_pointer) : "memory");

    asm volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        :
        :
        : "ax", "memory");
}
