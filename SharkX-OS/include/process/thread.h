#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_THREADS 32
#define THREAD_STACK_SIZE (16 * 1024)
#define THREAD_NAME_MAX 32

struct Mutex;

enum ThreadState
{
    THREAD_NEW = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
};

enum WaitKind
{
    WAIT_NONE = 0,
    WAIT_SLEEP,
    WAIT_MUTEX
};

struct Thread
{
    int id;
    char name[THREAD_NAME_MAX];
    ThreadState state;
    WaitKind wait_kind;
    bool is_idle;
    bool used;

    uint64_t rsp;
    uint8_t* stack_base;
    size_t stack_size;

    void (*entry)(void*);
    void* arg;

    uint64_t wake_tick;
    uint64_t wait_start_tick;
    Mutex* waiting_on;

    uint64_t cpu_ticks;
    uint64_t cpu_cycles;
    uint64_t switches;
    uint64_t created_tick;
};

void scheduler_init(void);
void scheduler_tick(void);
void schedule(void);

Thread* thread_create(const char* name, void (*entry)(void*), void* arg);
void thread_exit(void);
void thread_yield(void);
void thread_sleep_ticks(uint64_t ticks);
void thread_tick(void);

Thread* thread_current(void);
Thread* thread_get(int index);
int thread_max(void);
int thread_live_count(void);
uint64_t thread_switches(void);
const char* thread_state_name(ThreadState state);
const char* thread_wait_name(WaitKind kind);
