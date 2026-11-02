#include <kernel/kernel.h>
#include <kernel/console.h>
#include <process/thread.h>
#include <lib/printf.h>

namespace
{
volatile uint64_t g_demo_counter = 0;
volatile uint64_t g_demo_hash = 0;

void counter_task(void*)
{
    for (;;)
    {
        for (int i = 0; i < 200000; ++i)
        {
            ++g_demo_counter;
        }
        thread_sleep_ticks(1);
    }
}

void hash_task(void*)
{
    uint64_t hash = 1469598103934665603ULL;
    for (;;)
    {
        for (int i = 0; i < 200000; ++i)
        {
            hash ^= (uint64_t)(i + 1);
            hash *= 1099511628211ULL;
        }
        g_demo_hash = hash;
        thread_sleep_ticks(1);
    }
}
}

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();

void kernel_run_constructors(void)
{
    for (void (**constructor)() = __init_array_start; constructor < __init_array_end; ++constructor)
    {
        (*constructor)();
    }
}

void kernel_print_banner(void)
{
    kprintf("\n");
    kprintf("  ██████╗ ██╗  ██╗ █████╗ ██████╗ ██╗  ██╗██╗  ██╗    ██████╗ ███████╗\n");
    kprintf(" ██╔════╝ ██║  ██║██╔══██╗██╔══██╗██║ ██╔╝╚██╗██╔╝   ██╔═══██╗██╔════╝\n");
    kprintf(" ███████╗ ███████║███████║██████╔╝█████═╝  ╚███╔╝    ██║   ██║███████╗\n");
    kprintf(" ╚════██║ ██╔══██║██╔══██║██╔══██╗██╔═██╗  ██╔██╗    ██║   ██║╚════██║\n");
    kprintf(" ███████║ ██║  ██║██║  ██║██║  ██║██║  ██╗██╔╝ ██╗   ╚██████╔╝███████║\n");
    kprintf(" ╚══════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝    ╚═════╝ ╚══════╝\n");
    kprintf("\n");
    kprintf(" SharkX-OS %s - Independent x86_64 kernel\n", SHARKX_VERSION_STRING);
    kprintf(" Boot protocol: Multiboot2 / GRUB2    Architecture: x86_64\n");
    kprintf(" Console: serial COM1 + VGA text      Shell: SharkX Shell\n");
    kprintf("\n");
}

void kernel_start_demo_threads(void)
{
    Thread* counter = thread_create("counter", counter_task, nullptr);
    Thread* hasher = thread_create("hasher", hash_task, nullptr);

    if (counter == nullptr)
    {
        kprintf("[kernel] warning: could not create counter thread\n");
    }
    if (hasher == nullptr)
    {
        kprintf("[kernel] warning: could not create hasher thread\n");
    }
}

uint64_t kernel_demo_counter(void)
{
    return g_demo_counter;
}

uint64_t kernel_demo_hash(void)
{
    return g_demo_hash;
}
