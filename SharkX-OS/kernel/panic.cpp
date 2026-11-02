#include <kernel/panic.h>
#include <kernel/console.h>
#include <kernel/io.h>
#include <lib/printf.h>

void panic_va(const char* fmt, va_list args)
{
    cpu_cli();

    char message[512];
    kvsnprintf(message, sizeof(message), fmt, args);

    console_write("\n*** KERNEL PANIC ***\n");
    console_write(message);
    console_write("\n\nSystem halted. Reset or power off the machine.\n");

    cpu_halt_forever();
}

void panic(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    panic_va(fmt, args);
    va_end(args);
}
