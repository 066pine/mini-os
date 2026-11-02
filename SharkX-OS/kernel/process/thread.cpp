#include <process/thread.h>
#include <scheduler/scheduler.h>
#include <kernel/io.h>
#include <kernel/timer.h>
#include <sync/mutex.h>
#include <lib/string.h>

extern "C" void context_switch(uint64_t* old_rsp, uint64_t new_rsp);
extern "C" void thread_trampoline(void);

namespace
{
Thread g_threads[MAX_THREADS];
alignas(16) uint8_t g_stacks[MAX_THREADS][THREAD_STACK_SIZE];

Thread* g_current = nullptr;
int g_idle_index = -1;
uint64_t g_switch_count = 0;
uint64_t g_last_tsc = 0;
bool g_scheduler_ready = false;

void idle_entry(void*)
{
    for (;;)
    {
        asm volatile("sti; hlt; cli" : : : "memory");
    }
}

void prepare_stack(Thread* thread)
{
    uint64_t top = (uint64_t)(thread->stack_base + thread->stack_size);
    top &= ~0xFULL;

    uint64_t* stack = (uint64_t*)top;
    *--stack = (uint64_t)thread_trampoline;
    *--stack = 0;
    *--stack = 0;
    *--stack = (uint64_t)thread->entry;
    *--stack = (uint64_t)thread->arg;
    *--stack = 0;
    *--stack = 0;

    thread->rsp = (uint64_t)stack;
}

Thread* create_thread_internal(const char* name, void (*entry)(void*), void* arg, bool idle)
{
    int slot = -1;
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        if (!g_threads[i].used || g_threads[i].state == THREAD_TERMINATED)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        return nullptr;
    }

    Thread* thread = &g_threads[slot];
    memset(thread, 0, sizeof(Thread));

    thread->id = slot;
    thread->used = true;
    thread->state = THREAD_READY;
    thread->wait_kind = WAIT_NONE;
    thread->is_idle = idle;
    thread->stack_base = g_stacks[slot];
    thread->stack_size = THREAD_STACK_SIZE;
    thread->entry = entry;
    thread->arg = arg;
    thread->created_tick = timer_ticks();

    if (name != nullptr)
    {
        strncpy(thread->name, name, THREAD_NAME_MAX - 1);
        thread->name[THREAD_NAME_MAX - 1] = '\0';
    }
    else
    {
        strcpy(thread->name, "thread");
    }

    prepare_stack(thread);
    return thread;
}

Thread* pick_next(Thread* previous)
{
    int start = 0;
    if (previous != nullptr)
    {
        start = (previous->id + 1) % MAX_THREADS;
    }

    for (int i = 0; i < MAX_THREADS; ++i)
    {
        Thread* candidate = &g_threads[(start + i) % MAX_THREADS];
        if (candidate->used && candidate->state == THREAD_READY && !candidate->is_idle)
        {
            return candidate;
        }
    }

    if (previous != nullptr && previous->used && previous->state == THREAD_READY && !previous->is_idle)
    {
        return previous;
    }

    if (g_idle_index >= 0)
    {
        Thread* idle = &g_threads[g_idle_index];
        if (idle->used && idle->state == THREAD_READY)
        {
            return idle;
        }
    }

    return nullptr;
}
}

extern "C" void thread_exit_c(void)
{
    thread_exit();
}

void scheduler_init(void)
{
    memset(g_threads, 0, sizeof(g_threads));

    Thread* main_thread = &g_threads[0];
    main_thread->id = 0;
    main_thread->used = true;
    main_thread->state = THREAD_RUNNING;
    main_thread->wait_kind = WAIT_NONE;
    main_thread->is_idle = false;
    strcpy(main_thread->name, "main");
    main_thread->entry = nullptr;
    main_thread->created_tick = timer_ticks();

    g_current = main_thread;
    g_idle_index = -1;
    g_switch_count = 0;
    g_last_tsc = cpu_rdtsc();

    Thread* idle = create_thread_internal("idle", idle_entry, nullptr, true);
    if (idle != nullptr)
    {
        g_idle_index = idle->id;
    }

    g_scheduler_ready = true;
}

void scheduler_tick(void)
{
    if (!g_scheduler_ready || g_current == nullptr)
    {
        return;
    }

    ++g_current->cpu_ticks;
    schedule();
}

void schedule(void)
{
    if (!g_scheduler_ready)
    {
        return;
    }

    Thread* previous = g_current;

    uint64_t now_tsc = cpu_rdtsc();
    if (previous != nullptr)
    {
        previous->cpu_cycles += now_tsc - g_last_tsc;
    }
    g_last_tsc = now_tsc;

    if (previous != nullptr && previous->state == THREAD_RUNNING)
    {
        previous->state = THREAD_READY;
    }

    Thread* next = pick_next(previous);

    if (next == nullptr)
    {
        if (previous != nullptr && previous->state == THREAD_READY)
        {
            previous->state = THREAD_RUNNING;
            return;
        }

        for (;;)
        {
            asm volatile("sti; hlt; cli" : : : "memory");
        }
    }

    if (next == previous)
    {
        previous->state = THREAD_RUNNING;
        return;
    }

    next->state = THREAD_RUNNING;
    ++next->switches;
    ++g_switch_count;

    Thread* old = g_current;
    g_current = next;
    context_switch(&old->rsp, next->rsp);
}

Thread* thread_create(const char* name, void (*entry)(void*), void* arg)
{
    if (entry == nullptr)
    {
        return nullptr;
    }

    uint64_t flags = irq_save();
    Thread* thread = create_thread_internal(name, entry, arg, false);
    irq_restore(flags);
    return thread;
}

void thread_exit(void)
{
    uint64_t flags = irq_save();

    if (g_current != nullptr)
    {
        g_current->state = THREAD_TERMINATED;
        g_current->wait_kind = WAIT_NONE;
        g_current->waiting_on = nullptr;
    }

    schedule();
    irq_restore(flags);
    cpu_halt_forever();
}

void thread_yield(void)
{
    uint64_t flags = irq_save();
    schedule();
    irq_restore(flags);
}

void thread_sleep_ticks(uint64_t ticks)
{
    if (ticks == 0)
    {
        thread_yield();
        return;
    }

    uint64_t flags = irq_save();
    if (g_current == nullptr)
    {
        irq_restore(flags);
        return;
    }

    uint64_t now = timer_ticks();
    g_current->state = THREAD_WAITING;
    g_current->wait_kind = WAIT_SLEEP;
    g_current->wake_tick = now + ticks;
    g_current->wait_start_tick = now;

    schedule();
    irq_restore(flags);
}

void thread_tick(void)
{
    uint64_t now = timer_ticks();

    for (int i = 0; i < MAX_THREADS; ++i)
    {
        Thread* thread = &g_threads[i];
        if (thread->used &&
            thread->state == THREAD_WAITING &&
            thread->wait_kind == WAIT_SLEEP &&
            now >= thread->wake_tick)
        {
            thread->state = THREAD_READY;
            thread->wait_kind = WAIT_NONE;
            thread->wake_tick = 0;
        }
    }
}

Thread* thread_current(void)
{
    return g_current;
}

Thread* thread_get(int index)
{
    if (index < 0 || index >= MAX_THREADS)
    {
        return nullptr;
    }
    return &g_threads[index];
}

int thread_max(void)
{
    return MAX_THREADS;
}

int thread_live_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        if (g_threads[i].used && g_threads[i].state != THREAD_TERMINATED)
        {
            ++count;
        }
    }
    return count;
}

uint64_t thread_switches(void)
{
    return g_switch_count;
}

const char* thread_state_name(ThreadState state)
{
    switch (state)
    {
    case THREAD_NEW:
        return "NEW";
    case THREAD_READY:
        return "READY";
    case THREAD_RUNNING:
        return "RUNNING";
    case THREAD_WAITING:
        return "WAITING";
    case THREAD_TERMINATED:
        return "TERMINATED";
    default:
        return "UNKNOWN";
    }
}

const char* thread_wait_name(WaitKind kind)
{
    switch (kind)
    {
    case WAIT_SLEEP:
        return "sleep";
    case WAIT_MUTEX:
        return "mutex";
    default:
        return "-";
    }
}
