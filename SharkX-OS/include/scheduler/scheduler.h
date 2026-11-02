#pragma once

#include "../process/thread.h"

uint64_t scheduler_switches(void);
const char* scheduler_policy_name(void);
uint64_t scheduler_quantum_ticks(void);
