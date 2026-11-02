#include <sync/mutex.h>
#include <kernel/io.h>
#include <kernel/timer.h>
#include <lib/printf.h>

namespace
{
bool g_deadlock_detection = true;

bool g_watchdog_armed = false;
bool g_watchdog_fired = false;
Mutex* g_watchdog_first = nullptr;
Mutex* g_watchdog_second = nullptr;
uint64_t g_watchdog_start = 0;

void add_waiter(Mutex* mutex, Thread* thread)
{
    for (int i = 0; i < mutex->waiter_count; ++i)
    {
        if (mutex->waiters[i] == thread)
        {
            return;
        }
    }

    if (mutex->waiter_count < MAX_THREADS)
    {
        mutex->waiters[mutex->waiter_count++] = thread;
    }
}

Thread* remove_waiter_at(Mutex* mutex, int index)
{
    if (index < 0 || index >= mutex->waiter_count)
    {
        return nullptr;
    }

    Thread* thread = mutex->waiters[index];
    for (int i = index; i + 1 < mutex->waiter_count; ++i)
    {
        mutex->waiters[i] = mutex->waiters[i + 1];
    }
    --mutex->waiter_count;
    mutex->waiters[mutex->waiter_count] = nullptr;
    return thread;
}

Thread* take_first_waiter(Mutex* mutex)
{
    return remove_waiter_at(mutex, 0);
}

void wake_thread(Thread* thread)
{
    if (thread == nullptr)
    {
        return;
    }

    if (thread->state == THREAD_WAITING)
    {
        thread->state = THREAD_READY;
    }
    thread->wait_kind = WAIT_NONE;
    thread->waiting_on = nullptr;
    thread->wake_tick = 0;
}

void wake_all_waiters(Mutex* mutex)
{
    while (mutex->waiter_count > 0)
    {
        Thread* thread = take_first_waiter(mutex);
        wake_thread(thread);
    }
}
}

void mutex_init(Mutex* mutex, const char* name)
{
    mutex->name = name;
    mutex->locked = false;
    mutex->owner = nullptr;
    mutex->waiter_count = 0;
    mutex->acquisitions = 0;
    mutex->contentions = 0;

    for (int i = 0; i < MAX_THREADS; ++i)
    {
        mutex->waiters[i] = nullptr;
    }
}

bool mutex_would_deadlock(Thread* requester, Mutex* mutex)
{
    if (requester == nullptr || mutex == nullptr)
    {
        return false;
    }

    Thread* owner = mutex->owner;

    for (int depth = 0; depth < MAX_THREADS + 1; ++depth)
    {
        if (owner == nullptr)
        {
            return false;
        }
        if (owner == requester)
        {
            return true;
        }
        if (owner->wait_kind != WAIT_MUTEX || owner->waiting_on == nullptr)
        {
            return false;
        }
        owner = owner->waiting_on->owner;
    }

    return false;
}

bool mutex_lock(Mutex* mutex)
{
    if (mutex == nullptr)
    {
        return false;
    }

    Thread* self = thread_current();

    for (;;)
    {
        uint64_t flags = irq_save();

        if (!mutex->locked)
        {
            mutex->locked = true;
            mutex->owner = self;
            ++mutex->acquisitions;
            irq_restore(flags);
            return true;
        }

        if (g_deadlock_detection && mutex_would_deadlock(self, mutex))
        {
            kprintf("[mutex] refused lock '%s': circular wait detected\n",
                    mutex->name != nullptr ? mutex->name : "?");
            irq_restore(flags);
            return false;
        }

        ++mutex->contentions;
        add_waiter(mutex, self);

        if (self != nullptr)
        {
            self->state = THREAD_WAITING;
            self->wait_kind = WAIT_MUTEX;
            self->waiting_on = mutex;
            self->wait_start_tick = timer_ticks();
            self->wake_tick = 0;
        }

        schedule();
        irq_restore(flags);
    }
}

bool mutex_try_lock(Mutex* mutex)
{
    if (mutex == nullptr)
    {
        return false;
    }

    uint64_t flags = irq_save();
    if (mutex->locked)
    {
        irq_restore(flags);
        return false;
    }

    mutex->locked = true;
    mutex->owner = thread_current();
    ++mutex->acquisitions;
    irq_restore(flags);
    return true;
}

void mutex_unlock(Mutex* mutex)
{
    if (mutex == nullptr)
    {
        return;
    }

    uint64_t flags = irq_save();
    if (!mutex->locked)
    {
        irq_restore(flags);
        return;
    }

    mutex->locked = false;
    mutex->owner = nullptr;

    Thread* waiter = take_first_waiter(mutex);
    wake_thread(waiter);

    irq_restore(flags);

    if (waiter != nullptr)
    {
        thread_yield();
    }
}

void mutex_force_unlock(Mutex* mutex)
{
    if (mutex == nullptr)
    {
        return;
    }

    uint64_t flags = irq_save();
    mutex->locked = false;
    mutex->owner = nullptr;
    wake_all_waiters(mutex);
    irq_restore(flags);
}

bool mutex_deadlock_detection_enabled(void)
{
    return g_deadlock_detection;
}

void mutex_set_deadlock_detection(bool enabled)
{
    g_deadlock_detection = enabled;
}

void mutex_watchdog_arm(Mutex* first, Mutex* second)
{
    uint64_t flags = irq_save();
    g_watchdog_armed = true;
    g_watchdog_fired = false;
    g_watchdog_first = first;
    g_watchdog_second = second;
    g_watchdog_start = timer_ticks();
    irq_restore(flags);
}

void mutex_watchdog_disarm(void)
{
    uint64_t flags = irq_save();
    g_watchdog_armed = false;
    g_watchdog_first = nullptr;
    g_watchdog_second = nullptr;
    irq_restore(flags);
}

void mutex_watchdog_tick(void)
{
    if (!g_watchdog_armed)
    {
        return;
    }

    uint64_t now = timer_ticks();
    if (now - g_watchdog_start < 200)
    {
        return;
    }

    int waiting = 0;
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        Thread* thread = thread_get(i);
        if (thread == nullptr || !thread->used)
        {
            continue;
        }
        if (thread->state == THREAD_WAITING &&
            thread->wait_kind == WAIT_MUTEX &&
            (thread->waiting_on == g_watchdog_first || thread->waiting_on == g_watchdog_second))
        {
            ++waiting;
        }
    }

    if (waiting < 2)
    {
        if (now - g_watchdog_start > 600)
        {
            g_watchdog_armed = false;
        }
        return;
    }

    kprintf("\n[deadlock] WATCHDOG: two threads have been blocked for %llu ticks.\n",
            (unsigned long long)(now - g_watchdog_start));
    kprintf("[deadlock] Coffman conditions observed:\n");
    kprintf("  1. Mutual exclusion: both mutexes are held exclusively.\n");
    kprintf("  2. Hold and wait: each thread holds one mutex and waits for the other.\n");
    kprintf("  3. No preemption: mutexes are released only by their owner.\n");
    kprintf("  4. Circular wait: thread A -> m2, thread B -> m1.\n");
    kprintf("[deadlock] Recovery: force-releasing both mutexes and waking waiters.\n");

    mutex_force_unlock(g_watchdog_first);
    mutex_force_unlock(g_watchdog_second);

    g_watchdog_armed = false;
    g_watchdog_fired = true;
    g_deadlock_detection = true;
}

bool mutex_watchdog_fired(void)
{
    return g_watchdog_fired;
}
