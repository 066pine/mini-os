#include <kernel/shell.h>
#include <kernel/console.h>
#include <kernel/io.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>
#include <memory/heap.h>
#include <memory/pmm.h>
#include <process/thread.h>
#include <scheduler/scheduler.h>
#include <sync/mutex.h>
#include <filesystem/vfs.h>
#include <security/security.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <stdint.h>
#include <stdbool.h>

namespace
{
constexpr size_t SHELL_LINE_MAX = 256;

VfsFile g_ls_buffer[VFS_MAX_FILES];

volatile int g_counter_value = 0;
volatile int g_counter_workers = 0;
volatile bool g_counter_use_mutex = false;
volatile int g_counter_iterations = 0;
Mutex g_counter_mutex;

Mutex g_deadlock_first;
Mutex g_deadlock_second;
volatile int g_deadlock_workers = 0;
volatile int g_deadlock_mode = 0;

char* next_token(char** cursor)
{
    char* p = *cursor;
    while (*p == ' ' || *p == '\t')
    {
        ++p;
    }

    if (*p == '\0')
    {
        *cursor = p;
        return nullptr;
    }

    char* token = p;
    while (*p != '\0' && *p != ' ' && *p != '\t')
    {
        ++p;
    }

    if (*p != '\0')
    {
        *p = '\0';
        ++p;
    }

    *cursor = p;
    return token;
}

void lowercase(char* text)
{
    while (*text != '\0')
    {
        if (*text >= 'A' && *text <= 'Z')
        {
            *text = (char)(*text - 'A' + 'a');
        }
        ++text;
    }
}

void shell_read_line(char* buffer, size_t capacity)
{
    size_t length = 0;
    buffer[0] = '\0';

    for (;;)
    {
        int input = console_getchar();
        if (input < 0)
        {
            thread_yield();
            continue;
        }

        char c = (char)input;

        if (c == '\r' || c == '\n')
        {
            kprintf("\n");
            buffer[length] = '\0';
            return;
        }

        if (c == '\b' || c == 127)
        {
            if (length > 0)
            {
                --length;
                buffer[length] = '\0';
                console_write("\b \b");
            }
            continue;
        }

        if (c == 3)
        {
            length = 0;
            buffer[0] = '\0';
            console_write("^C\n");
            return;
        }

        if (c == '\t')
        {
            c = ' ';
        }

        if (c >= 32 && c <= 126 && length + 1 < capacity)
        {
            buffer[length++] = c;
            buffer[length] = '\0';
            console_putc(c);
        }
    }
}

void print_usage(const char* usage)
{
    kprintf("usage: %s\n", usage);
}

void command_help(void)
{
    kprintf("SharkX Shell commands\n");
    kprintf("  System : help about version clear uptime meminfo heap-test halt reboot exit\n");
    kprintf("  Threads: threads ps scheduler\n");
    kprintf("  Sync   : mutex-test race-test deadlock-test deadlock-prevent\n");
    kprintf("  Files  : ls touch cat write rm permissions\n");
    kprintf("  Security: whoami su chmod\n");
    kprintf("  Misc   : echo <text>\n");
}

void command_about(void)
{
    kprintf("SharkX OS %s\n", SHARKX_VERSION_STRING);
    kprintf("An educational, independent x86_64 kernel written in C++ and NASM.\n");
    kprintf("Boot: GRUB2 Multiboot2 -> 64-bit long mode -> monolithic kernel.\n");
    kprintf("Subsystems: GDT/IDT/PIC/PIT, PMM, heap, threads, round-robin\n");
    kprintf("scheduler, mutexes, RAMDisk VFS, software DAC, serial+VGA console.\n");
}

void command_meminfo(void)
{
    HeapStats heap = heap_stats();
    const PmmStats& pmm = pmm_stats();

    kprintf("Kernel heap allocator\n");
    kprintf("  total      : %llu bytes (%llu KiB)\n",
            (unsigned long long)heap.total_bytes,
            (unsigned long long)(heap.total_bytes / 1024));
    kprintf("  used       : %llu bytes\n", (unsigned long long)heap.used_bytes);
    kprintf("  available  : %llu bytes\n", (unsigned long long)heap.free_bytes);
    kprintf("  largest    : %llu bytes\n", (unsigned long long)heap.largest_free_block);
    kprintf("  blocks     : %llu (%llu free)\n",
            (unsigned long long)heap.block_count,
            (unsigned long long)heap.free_block_count);
    kprintf("  alloc/free : %llu / %llu, failures=%llu, double-free=%llu\n",
            (unsigned long long)heap.allocations,
            (unsigned long long)heap.frees,
            (unsigned long long)heap.failures,
            (unsigned long long)heap.double_free_detected);

    kprintf("Physical memory\n");
    kprintf("  Multiboot address max : %llu MiB\n",
            (unsigned long long)(pmm.multiboot_total_bytes / (1024 * 1024)));
    kprintf("  Multiboot available   : %llu MiB\n",
            (unsigned long long)(pmm.multiboot_available_bytes / (1024 * 1024)));
    kprintf("  Frame allocator       : %llu frames total, %llu free, %llu used\n",
            (unsigned long long)pmm.total_frames,
            (unsigned long long)pmm.free_frames,
            (unsigned long long)pmm.used_frames);
    kprintf("  Free physical memory  : %llu MiB\n",
            (unsigned long long)(pmm.free_frames * PAGE_SIZE / (1024 * 1024)));
    kprintf("  Kernel image          : 0x%llx - 0x%llx (%llu KiB)\n",
            (unsigned long long)pmm.kernel_start,
            (unsigned long long)pmm.kernel_end,
            (unsigned long long)((pmm.kernel_end - pmm.kernel_start) / 1024));
}

uint64_t thread_cpu_ms(const Thread* thread)
{
    uint64_t hz = timer_tsc_hz();
    if (hz == 0)
    {
        return 0;
    }
    return (thread->cpu_cycles * 1000ULL) / hz;
}

void command_threads(bool live_only)
{
    kprintf(" id  name             state        cpu-ms switches wait\n");

    for (int i = 0; i < thread_max(); ++i)
    {
        Thread* thread = thread_get(i);
        if (thread == nullptr || !thread->used)
        {
            continue;
        }
        if (live_only && thread->state == THREAD_TERMINATED)
        {
            continue;
        }

        kprintf("%3d  %-16s %-11s %9llu %8llu %s\n",
                thread->id,
                thread->name,
                thread_state_name(thread->state),
                (unsigned long long)thread_cpu_ms(thread),
                (unsigned long long)thread->switches,
                thread_wait_name(thread->wait_kind));
    }

    kprintf("live threads: %d, context switches: %llu\n",
            thread_live_count(),
            (unsigned long long)thread_switches());
}

void command_scheduler(void)
{
    uint64_t ticks = timer_ticks();
    uint64_t seconds = ticks / TIMER_HZ;
    uint64_t hundredths = (ticks % TIMER_HZ) * 100 / TIMER_HZ;

    kprintf("Scheduler policy : %s\n", scheduler_policy_name());
    kprintf("Quantum          : %llu tick(s) at %u Hz\n",
            (unsigned long long)scheduler_quantum_ticks(),
            (unsigned int)TIMER_HZ);
    kprintf("Uptime           : %llu ticks (%llu.%02llu s)\n",
            (unsigned long long)ticks,
            (unsigned long long)seconds,
            (unsigned long long)hundredths);
    kprintf("Context switches : %llu\n", (unsigned long long)scheduler_switches());
    kprintf("TSC frequency    : %llu MHz\n",
            (unsigned long long)(timer_tsc_hz() / 1000000ULL));
    kprintf("Demo work        : counter=%llu hasher=0x%llx\n",
            (unsigned long long)kernel_demo_counter(),
            (unsigned long long)kernel_demo_hash());
    kprintf("Per-thread CPU time:\n");

    for (int i = 0; i < thread_max(); ++i)
    {
        Thread* thread = thread_get(i);
        if (thread == nullptr || !thread->used || thread->state == THREAD_TERMINATED)
        {
            continue;
        }
        kprintf("  %-16s %9llu ms  switches=%llu  state=%s\n",
                thread->name,
                (unsigned long long)thread_cpu_ms(thread),
                (unsigned long long)thread->switches,
                thread_state_name(thread->state));
    }
}

void command_ls(void)
{
    int count = vfs_list(g_ls_buffer, VFS_MAX_FILES);
    kprintf("mode  owner    size  name\n");

    for (int i = 0; i < count; ++i)
    {
        VfsFile* file = &g_ls_buffer[i];
        kprintf("%04o  %-7s %5llu  %s\n",
                (unsigned int)file->mode,
                security_name_for_uid(file->owner),
                (unsigned long long)file->size,
                file->name);
    }

    kprintf("%d file(s), current user: %s\n", count, security_current_name());
}

void command_permissions(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);

    if (name == nullptr)
    {
        int count = vfs_list(g_ls_buffer, VFS_MAX_FILES);
        uint32_t uid = security_current_uid();
        for (int i = 0; i < count; ++i)
        {
            VfsFile* file = &g_ls_buffer[i];
            kprintf("%-20s mode=%04o owner=%-7s read=%s write=%s\n",
                    file->name,
                    (unsigned int)file->mode,
                    security_name_for_uid(file->owner),
                    vfs_can_access(file, uid, VFS_PERM_READ) ? "yes" : "no",
                    vfs_can_access(file, uid, VFS_PERM_WRITE) ? "yes" : "no");
        }
        return;
    }

    VfsFile info;
    int result = vfs_stat(name, &info);
    if (result != VFS_OK)
    {
        kprintf("permissions: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    uint32_t uid = security_current_uid();
    kprintf("file        : %s\n", info.name);
    kprintf("owner       : %s (uid %u)\n", security_name_for_uid(info.owner), (unsigned int)info.owner);
    kprintf("mode        : %04o\n", (unsigned int)info.mode);
    kprintf("size        : %llu bytes\n", (unsigned long long)info.size);
    kprintf("read        : %s\n", vfs_can_access(&info, uid, VFS_PERM_READ) ? "allowed" : "DENIED");
    kprintf("write/delete: %s\n", vfs_can_access(&info, uid, VFS_PERM_WRITE) ? "allowed" : "DENIED");
}

void command_cat(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);
    if (name == nullptr)
    {
        print_usage("cat <file>");
        return;
    }

    char buffer[VFS_DATA_MAX + 1];
    size_t size = 0;
    int result = vfs_read(name, buffer, sizeof(buffer), &size, security_current_uid());
    if (result != VFS_OK)
    {
        kprintf("cat: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    console_write_n(buffer, size);
    if (size == 0 || buffer[size - 1] != '\n')
    {
        kprintf("\n");
    }
}

void command_write(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);
    if (name == nullptr)
    {
        print_usage("write <file> <text>");
        return;
    }

    while (*cursor == ' ' || *cursor == '\t')
    {
        ++cursor;
    }
    char* content = cursor;
    size_t length = strlen(content);
    int result = vfs_write(name, content, length, security_current_uid());

    if (result != VFS_OK)
    {
        kprintf("write: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    kprintf("wrote %llu byte(s) to %s\n", (unsigned long long)length, name);
}

void command_touch(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);
    if (name == nullptr)
    {
        print_usage("touch <file>");
        return;
    }

    int result = vfs_create(name, security_current_uid(), 0644);
    if (result != VFS_OK)
    {
        kprintf("touch: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    kprintf("created %s (owner %s, mode 0644)\n", name, security_current_name());
}

void command_rm(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);
    if (name == nullptr)
    {
        print_usage("rm <file>");
        return;
    }

    int result = vfs_delete(name, security_current_uid());
    if (result != VFS_OK)
    {
        kprintf("rm: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    kprintf("removed %s\n", name);
}

void command_whoami(void)
{
    kprintf("%s (uid %u)\n", security_current_name(), (unsigned int)security_current_uid());
}

void command_su(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);

    if (name == nullptr)
    {
        kprintf("users: root (uid 0), guest (uid 1)\n");
        print_usage("su <root|guest>");
        return;
    }

    if (!security_login(name))
    {
        kprintf("su: unknown user '%s' (available: root, guest)\n", name);
        return;
    }

    kprintf("now running as %s (uid %u)\n", security_current_name(), (unsigned int)security_current_uid());
}

void command_chmod(char* args)
{
    char* cursor = args;
    char* name = next_token(&cursor);
    char* mode_text = next_token(&cursor);

    if (name == nullptr || mode_text == nullptr)
    {
        print_usage("chmod <file> <octal-mode>");
        return;
    }

    if (!security_is_root())
    {
        kprintf("chmod: permission denied (root only)\n");
        return;
    }

    long mode = strtol(mode_text, nullptr, 8);
    if (mode < 0 || mode > 0777)
    {
        kprintf("chmod: invalid mode '%s'\n", mode_text);
        return;
    }

    int result = vfs_chmod(name, (uint16_t)mode, security_current_uid());
    if (result != VFS_OK)
    {
        kprintf("chmod: %s: %s\n", name, vfs_error_name(result));
        return;
    }

    kprintf("mode of %s changed to %04o\n", name, (unsigned int)mode);
}

void counter_worker(void*)
{
    for (int i = 0; i < g_counter_iterations; ++i)
    {
        if (g_counter_use_mutex)
        {
            mutex_lock(&g_counter_mutex);
            int value = g_counter_value;
            thread_yield();
            g_counter_value = value + 1;
            mutex_unlock(&g_counter_mutex);
        }
        else
        {
            int value = g_counter_value;
            thread_yield();
            g_counter_value = value + 1;
        }
    }

    uint64_t flags = irq_save();
    --g_counter_workers;
    irq_restore(flags);
}

int run_counter_test(bool use_mutex, int iterations, const char* label)
{
    mutex_init(&g_counter_mutex, "counter-mutex");
    g_counter_value = 0;
    g_counter_use_mutex = use_mutex;
    g_counter_iterations = iterations;
    g_counter_workers = 2;

    Thread* first = thread_create("count-a", counter_worker, nullptr);
    Thread* second = thread_create("count-b", counter_worker, nullptr);

    if (first == nullptr || second == nullptr)
    {
        kprintf("[%s] could not create worker threads\n", label);
        g_counter_workers = 0;
        return -1;
    }

    uint64_t start = timer_ticks();
    while (g_counter_workers > 0 && timer_ticks() - start < 1000)
    {
        thread_yield();
    }

    int expected = iterations * 2;
    kprintf("[%s] expected=%d actual=%d %s\n",
            label,
            expected,
            g_counter_value,
            g_counter_value == expected ? "CONSISTENT" : "LOST UPDATES");

    if (g_counter_workers != 0)
    {
        kprintf("[%s] timeout with %d worker(s) remaining\n", label, g_counter_workers);
    }

    return g_counter_value;
}

void command_mutex_test(void)
{
    kprintf("Two threads increment a shared counter 1000 times each under a mutex.\n");
    int value = run_counter_test(true, 1000, "mutex-test");
    kprintf("mutex acquisitions=%llu contentions=%llu\n",
            (unsigned long long)g_counter_mutex.acquisitions,
            (unsigned long long)g_counter_mutex.contentions);

    if (value == 2000)
    {
        kprintf("mutex-test: synchronization produced a deterministic result\n");
    }
}

void command_race_test(void)
{
    kprintf("Unsynchronized run: two threads perform read/yield/write on a shared counter.\n");
    int unsafe = run_counter_test(false, 1000, "race-unsafe");

    kprintf("Synchronized run: the same workload protected by a mutex.\n");
    int safe = run_counter_test(true, 1000, "race-safe");

    if (unsafe != 2000)
    {
        kprintf("race-test: unsafe result differs from expected 2000 (race demonstrated)\n");
    }
    if (safe == 2000)
    {
        kprintf("race-test: mutex result is consistent at 2000\n");
    }
}

void deadlock_worker(void* argument)
{
    int which = (int)(intptr_t)argument;
    int mode = g_deadlock_mode;

    if (mode == 0)
    {
        Mutex* first = (which == 0) ? &g_deadlock_first : &g_deadlock_second;
        Mutex* second = (which == 0) ? &g_deadlock_second : &g_deadlock_first;

        kprintf("[deadlock] %c acquires %s\n", which == 0 ? 'A' : 'B', first->name);
        mutex_lock(first);
        thread_sleep_ticks(100);

        kprintf("[deadlock] %c waits for %s\n", which == 0 ? 'A' : 'B', second->name);
        bool acquired = mutex_lock(second);
        if (acquired)
        {
            kprintf("[deadlock] %c acquired %s after watchdog recovery\n",
                    which == 0 ? 'A' : 'B', second->name);
            mutex_unlock(second);
        }
        mutex_unlock(first);
    }
    else
    {
        kprintf("[deadlock] %c acquires m1, then m2 (fixed lock order)\n", which == 0 ? 'A' : 'B');
        mutex_lock(&g_deadlock_first);
        thread_sleep_ticks(10);
        mutex_lock(&g_deadlock_second);
        mutex_unlock(&g_deadlock_second);
        mutex_unlock(&g_deadlock_first);
        kprintf("[deadlock] %c completed without deadlock\n", which == 0 ? 'A' : 'B');
    }

    uint64_t flags = irq_save();
    --g_deadlock_workers;
    irq_restore(flags);
}

void command_deadlock_test(void)
{
    mutex_init(&g_deadlock_first, "m1");
    mutex_init(&g_deadlock_second, "m2");

    g_deadlock_mode = 0;
    g_deadlock_workers = 2;
    mutex_set_deadlock_detection(false);
    mutex_watchdog_arm(&g_deadlock_first, &g_deadlock_second);

    kprintf("Starting two threads with opposite lock order (detection disabled).\n");
    Thread* first = thread_create("deadlock-a", deadlock_worker, (void*)0);
    Thread* second = thread_create("deadlock-b", deadlock_worker, (void*)1);

    if (first == nullptr || second == nullptr)
    {
        kprintf("[deadlock-test] could not create workers\n");
        mutex_watchdog_disarm();
        mutex_set_deadlock_detection(true);
        g_deadlock_workers = 0;
        return;
    }

    uint64_t start = timer_ticks();
    while (g_deadlock_workers > 0 && timer_ticks() - start < 800)
    {
        thread_yield();
    }

    bool fired = mutex_watchdog_fired();
    mutex_watchdog_disarm();
    mutex_set_deadlock_detection(true);

    if (g_deadlock_workers > 0)
    {
        kprintf("[deadlock-test] timeout, forcing recovery\n");
        mutex_force_unlock(&g_deadlock_first);
        mutex_force_unlock(&g_deadlock_second);
    }

    kprintf("[deadlock-test] result: %s\n",
            fired ? "actual deadlock observed, watchdog recovered both threads"
                  : "no deadlock observed");
}

void command_deadlock_prevent(void)
{
    mutex_init(&g_deadlock_first, "m1");
    mutex_init(&g_deadlock_second, "m2");

    g_deadlock_mode = 1;
    g_deadlock_workers = 2;
    mutex_set_deadlock_detection(true);

    kprintf("Starting two threads using lock ordering m1 -> m2.\n");
    Thread* first = thread_create("prevent-a", deadlock_worker, (void*)0);
    Thread* second = thread_create("prevent-b", deadlock_worker, (void*)1);

    if (first == nullptr || second == nullptr)
    {
        kprintf("[deadlock-prevent] could not create workers\n");
        g_deadlock_workers = 0;
        return;
    }

    uint64_t start = timer_ticks();
    while (g_deadlock_workers > 0 && timer_ticks() - start < 600)
    {
        thread_yield();
    }

    if (g_deadlock_workers == 0)
    {
        kprintf("deadlock-prevent: both threads completed; lock ordering avoided circular wait\n");
    }
    else
    {
        kprintf("deadlock-prevent: timeout, unexpected state\n");
    }
}

void command_uptime(void)
{
    uint64_t ticks = timer_ticks();
    kprintf("uptime: %llu ticks (%llu.%02llu seconds at %u Hz)\n",
            (unsigned long long)ticks,
            (unsigned long long)(ticks / TIMER_HZ),
            (unsigned long long)((ticks % TIMER_HZ) * 100 / TIMER_HZ),
            (unsigned int)TIMER_HZ);
}

void execute_command(char* line)
{
    trim(line);
    if (line[0] == '\0')
    {
        return;
    }

    char* command = line;
    char* cursor = line;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
    {
        ++cursor;
    }
    if (*cursor != '\0')
    {
        *cursor = '\0';
        ++cursor;
        while (*cursor == ' ' || *cursor == '\t')
        {
            ++cursor;
        }
    }

    char* args = cursor;
    lowercase(command);

    if (strcmp(command, "help") == 0)
    {
        command_help();
    }
    else if (strcmp(command, "about") == 0)
    {
        command_about();
    }
    else if (strcmp(command, "version") == 0)
    {
        kprintf("SharkX OS v%s (x86_64, Multiboot2)\n", SHARKX_VERSION_STRING);
    }
    else if (strcmp(command, "clear") == 0)
    {
        console_clear();
    }
    else if (strcmp(command, "uptime") == 0)
    {
        command_uptime();
    }
    else if (strcmp(command, "meminfo") == 0)
    {
        command_meminfo();
    }
    else if (strcmp(command, "heap-test") == 0)
    {
        heap_selftest();
    }
    else if (strcmp(command, "threads") == 0)
    {
        command_threads(false);
    }
    else if (strcmp(command, "ps") == 0)
    {
        command_threads(true);
    }
    else if (strcmp(command, "scheduler") == 0)
    {
        command_scheduler();
    }
    else if (strcmp(command, "mutex-test") == 0)
    {
        command_mutex_test();
    }
    else if (strcmp(command, "race-test") == 0)
    {
        command_race_test();
    }
    else if (strcmp(command, "deadlock-test") == 0)
    {
        command_deadlock_test();
    }
    else if (strcmp(command, "deadlock-prevent") == 0)
    {
        command_deadlock_prevent();
    }
    else if (strcmp(command, "ls") == 0)
    {
        command_ls();
    }
    else if (strcmp(command, "touch") == 0)
    {
        command_touch(args);
    }
    else if (strcmp(command, "cat") == 0)
    {
        command_cat(args);
    }
    else if (strcmp(command, "write") == 0)
    {
        command_write(args);
    }
    else if (strcmp(command, "rm") == 0 || strcmp(command, "delete") == 0)
    {
        command_rm(args);
    }
    else if (strcmp(command, "permissions") == 0)
    {
        command_permissions(args);
    }
    else if (strcmp(command, "whoami") == 0)
    {
        command_whoami();
    }
    else if (strcmp(command, "su") == 0 || strcmp(command, "login") == 0)
    {
        command_su(args);
    }
    else if (strcmp(command, "chmod") == 0)
    {
        command_chmod(args);
    }
    else if (strcmp(command, "echo") == 0)
    {
        kprintf("%s\n", args);
    }
    else if (strcmp(command, "halt") == 0)
    {
        kprintf("Halting SharkX OS. Use 'reboot' or reset the VM.\n");
        cpu_halt_forever();
    }
    else if (strcmp(command, "reboot") == 0)
    {
        kprintf("Rebooting via the keyboard controller...\n");
        while ((inb(0x64) & 0x02) != 0)
        {
            cpu_pause();
        }
        outb(0x64, 0xFE);
        io_wait();
        outb(0xCF9, 0x06);
        cpu_halt_forever();
    }
    else if (strcmp(command, "exit") == 0)
    {
        kprintf("Triggering QEMU isa-debug-exit (test helper)...\n");
        outl(0x501, 0x10);
        cpu_halt_forever();
    }
    else
    {
        kprintf("Unknown command '%s'. Type 'help'.\n", command);
    }
}
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    kprintf("SharkX Shell ready. Current user: %s.\n", security_current_name());

    for (;;)
    {
        kprintf("SharkX> ");
        shell_read_line(line, sizeof(line));
        execute_command(line);
    }
}
