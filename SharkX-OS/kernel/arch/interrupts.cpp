#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/timer.h>
#include <kernel/console.h>
#include <kernel/panic.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <lib/printf.h>

namespace
{
const char* const g_exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

void exception_panic(InterruptRegisters* regs)
{
    uint64_t cr2 = 0;
    if (regs->vector == 14)
    {
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
    }

    kprintf("\n*** EXCEPTION %llu: %s ***\n",
            (unsigned long long)regs->vector,
            g_exception_names[regs->vector & 31]);

    if (regs->vector == 14)
    {
        kprintf("CR2 (faulting address): 0x%llx\n", (unsigned long long)cr2);
    }

    kprintf("RIP: 0x%llx  CS: 0x%llx  RFLAGS: 0x%llx\n",
            (unsigned long long)regs->rip,
            (unsigned long long)regs->cs,
            (unsigned long long)regs->rflags);
    kprintf("RSP: 0x%llx  SS: 0x%llx  ERROR: 0x%llx\n",
            (unsigned long long)regs->rsp,
            (unsigned long long)regs->ss,
            (unsigned long long)regs->error);
    kprintf("RAX: 0x%llx  RBX: 0x%llx  RCX: 0x%llx  RDX: 0x%llx\n",
            (unsigned long long)regs->rax,
            (unsigned long long)regs->rbx,
            (unsigned long long)regs->rcx,
            (unsigned long long)regs->rdx);
    kprintf("RSI: 0x%llx  RDI: 0x%llx  RBP: 0x%llx\n",
            (unsigned long long)regs->rsi,
            (unsigned long long)regs->rdi,
            (unsigned long long)regs->rbp);

    panic("unrecoverable CPU exception");
}
}

extern "C" void isr_dispatch(InterruptRegisters* regs)
{
    if (regs == nullptr)
    {
        panic("null interrupt frame");
    }

    uint64_t vector = regs->vector;

    if (vector < 32)
    {
        exception_panic(regs);
    }

    if (vector >= 32 && vector < 48)
    {
        uint8_t irq = (uint8_t)(vector - 32);
        pic_eoi(irq);

        if (irq == 0)
        {
            timer_irq_handler();
        }
        else if (irq == 1)
        {
            keyboard_irq_handler();
        }
        else if (irq == 4)
        {
            serial_irq_handler();
        }
        return;
    }

    kprintf("\nUnexpected interrupt vector %llu\n", (unsigned long long)vector);
    panic("unexpected interrupt");
}
