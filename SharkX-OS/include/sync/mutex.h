#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../process/thread.h"

struct Mutex
{
    const char* name;
    bool locked;
    Thread* owner;
    Thread* waiters[MAX_THREADS];
    int waiter_count;
    uint64_t acquisitions;
    uint64_t contentions;
};

#define MUTEX_INITIALIZER(name_literal) \
    { name_literal, false, nullptr, { nullptr }, 0, 0, 0 }

void mutex_init(Mutex* mutex, const char* name);
bool mutex_lock(Mutex* mutex);
bool mutex_try_lock(Mutex* mutex);
void mutex_unlock(Mutex* mutex);
void mutex_force_unlock(Mutex* mutex);

bool mutex_deadlock_detection_enabled(void);
void mutex_set_deadlock_detection(bool enabled);
bool mutex_would_deadlock(Thread* requester, Mutex* mutex);

void mutex_watchdog_arm(Mutex* first, Mutex* second);
void mutex_watchdog_disarm(void);
void mutex_watchdog_tick(void);
bool mutex_watchdog_fired(void);
