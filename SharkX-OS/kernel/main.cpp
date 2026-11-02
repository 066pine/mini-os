#include <kernel/types.h>
#include <kernel/io.h>
#include <kernel/console.h>
#include <kernel/panic.h>
#include <kernel/kernel.h>
#include <kernel/multiboot2.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/timer.h>
#include <kernel/shell.h>
#include <drivers/keyboard.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <process/thread.h>
#include <security/security.h>
#include <filesystem/vfs.h>
#include <lib/printf.h>

extern "C" void kernel_main(uint32_t magic, uint64_t mbi_addr)
{
    console_init();
    kernel_print_banner();

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        panic("invalid Multiboot2 magic: 0x%x (expected 0x%x)",
              (unsigned int)magic,
              (unsigned int)MULTIBOOT2_BOOTLOADER_MAGIC);
    }

    if (mbi_addr == 0)
    {
        panic("Multiboot2 information pointer is null");
    }

    kprintf("[boot] Multiboot2 magic valid, info at 0x%llx\n", (unsigned long long)mbi_addr);

    const char* cmdline = multiboot_cmdline(mbi_addr);
    if (cmdline != nullptr && cmdline[0] != '\0')
    {
        kprintf("[boot] command line: %s\n", cmdline);
    }

    gdt_init();
    kprintf("[boot] GDT loaded\n");

    idt_init();
    kprintf("[boot] IDT loaded (exceptions + IRQs 0-15)\n");

    pic_remap();
    pic_mask_all();
    pic_unmask(0);
    pic_unmask(1);
    pic_unmask(4);
    timer_init(TIMER_HZ);
    keyboard_init();
    kprintf("[boot] PIC remapped, PIT at %u Hz, PS/2 keyboard + serial IRQ ready\n", (unsigned int)TIMER_HZ);

    pmm_init(mbi_addr);
    const PmmStats& pmm = pmm_stats();
    kprintf("[boot] PMM: %llu frames managed, %llu free (%llu KiB)\n",
            (unsigned long long)pmm.total_frames,
            (unsigned long long)pmm.free_frames,
            (unsigned long long)(pmm.free_frames * PAGE_SIZE / 1024));

    heap_init();
    HeapStats heap = heap_stats();
    kprintf("[boot] kernel heap: %llu KiB\n", (unsigned long long)(heap.total_bytes / 1024));

    security_init();
    kprintf("[boot] security: software DAC active, current user %s\n", security_current_name());

    vfs_init();
    kprintf("[boot] VFS RAMDisk: %d files\n", vfs_count());

    kernel_run_constructors();

    scheduler_init();
    kernel_start_demo_threads();
    kprintf("[boot] scheduler: round robin, %d live kernel threads (incl. main and idle)\n", thread_live_count());

    kprintf("\n[ready] Type 'help' for the SharkX Shell command list.\n\n");

    cpu_sti();
    shell_run();

    panic("shell returned unexpectedly");
}
