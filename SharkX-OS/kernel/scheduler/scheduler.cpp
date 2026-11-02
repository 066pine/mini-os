#include <scheduler/scheduler.h>
#include <kernel/timer.h>

uint64_t scheduler_switches(void)
{
    return thread_switches();
}

const char* scheduler_policy_name(void)
{
    return "Round Robin";
}

uint64_t scheduler_quantum_ticks(void)
{
    return 1;
}
