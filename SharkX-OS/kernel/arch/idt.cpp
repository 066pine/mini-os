#include <kernel/idt.h>
#include <kernel/io.h>
#include <lib/string.h>
#include <stdint.h>

namespace
{
struct IdtEntry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct IdtPointer
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

IdtEntry g_idt[256];
IdtPointer g_idt_pointer;

extern "C" uint64_t isr_stub_table[];
extern "C" void isr_default(void);
}

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t attributes)
{
    IdtEntry& entry = g_idt[vector];
    entry.offset_low = (uint16_t)(handler & 0xFFFF);
    entry.selector = 0x08;
    entry.ist = ist & 0x07;
    entry.attributes = attributes;
    entry.offset_middle = (uint16_t)((handler >> 16) & 0xFFFF);
    entry.offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    entry.reserved = 0;
}

void idt_init(void)
{
    memset(g_idt, 0, sizeof(g_idt));

    for (int i = 0; i < 256; ++i)
    {
        idt_set_gate((uint8_t)i, (uint64_t)isr_default, 0, 0x8E);
    }

    for (int i = 0; i < 48; ++i)
    {
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0, 0x8E);
    }

    g_idt_pointer.limit = (uint16_t)(sizeof(g_idt) - 1);
    g_idt_pointer.base = (uint64_t)&g_idt[0];

    asm volatile("lidt %0" : : "m"(g_idt_pointer) : "memory");
}
