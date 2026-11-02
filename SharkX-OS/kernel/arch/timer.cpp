#include <kernel/timer.h>
#include <kernel/io.h>
#include <process/thread.h>
#include <sync/mutex.h>

namespace
{
constexpr uint16_t PIT_CHANNEL0 = 0x40;
constexpr uint16_t PIT_COMMAND = 0x43;
constexpr uint32_t PIT_BASE_FREQUENCY = 1193182;

volatile uint64_t g_ticks = 0;
uint64_t g_tsc_hz = 1000000000ULL;

uint64_t calibrate_tsc(void)
{
    constexpr uint32_t calibration_ms = 50;
    constexpr uint32_t calibration_count = (PIT_BASE_FREQUENCY / 1000) * calibration_ms;

    uint8_t port61 = inb(0x61);
    outb(0x61, (uint8_t)(port61 & ~0x03));

    outb(PIT_COMMAND, 0xB0);
    outb(0x42, (uint8_t)(calibration_count & 0xFF));
    outb(0x42, (uint8_t)((calibration_count >> 8) & 0xFF));

    outb(0x61, (uint8_t)((port61 & ~0x02) | 0x01));

    uint64_t start = cpu_rdtsc();
    uint64_t limit = start + 20000000000ULL;
    while ((inb(0x61) & 0x20) == 0)
    {
        if (cpu_rdtsc() > limit)
        {
            outb(0x61, port61);
            return 1000000000ULL;
        }
    }
    uint64_t end = cpu_rdtsc();
    outb(0x61, port61);

    uint64_t elapsed = end - start;
    if (elapsed < 1000)
    {
        return 1000000000ULL;
    }

    return elapsed * (1000 / calibration_ms);
}
}

void timer_init(uint32_t frequency)
{
    if (frequency == 0)
    {
        frequency = TIMER_HZ;
    }

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;
    if (divisor > 0xFFFF)
    {
        divisor = 0xFFFF;
    }
    if (divisor == 0)
    {
        divisor = 1;
    }

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    g_tsc_hz = calibrate_tsc();
}

uint64_t timer_ticks(void)
{
    return g_ticks;
}

uint64_t timer_tsc_hz(void)
{
    return g_tsc_hz;
}

void timer_irq_handler(void)
{
    ++g_ticks;
    thread_tick();
    mutex_watchdog_tick();
    scheduler_tick();
}
