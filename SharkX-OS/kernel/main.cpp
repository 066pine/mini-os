
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    asm volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

void serial_init()
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write(char c)
{
    while (!(inb(COM1 + 5) & 0x20));

    outb(COM1, c);
}

void print(const char* text)
{
    while (*text)
    {
        serial_write(*text++);
    }
}

void println(const char* text)
{
    print(text);
    print("\r\n");
}

void read_line(char* buffer, int max)
{
    int i = 0;

    while (true)
    {
        char c = 0;

        while (!(inb(COM1 + 5) & 0x01));

        c = inb(COM1);

        if (c == '\r' || c == '\n')
        {
            print("\r\n");
            break;
        }

        if (c == '\b' || c == 127)
        {
            if (i > 0)
            {
                i--;
                print("\b \b");
            }
            continue;
        }

        if (i < max - 1 && c >= 32 && c <= 126)
        {
            buffer[i++] = c;
            serial_write(c);
        }
    }

    buffer[i] = '\0';
}

bool equals(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;

        a++;
        b++;
    }

    return *a == *b;
}

void shell()
{
    char command[128];

    while (true)
    {
        print("SharkX> ");

        read_line(command, sizeof(command));

        if (equals(command, "help"))
        {
            println("Available commands:");
            println("help    - Show commands");
            println("about   - OS information");
            println("version - Kernel version");
            println("clear   - Clear terminal");
            println("halt    - Stop CPU");
        }
        else if (equals(command, "about"))
        {
            println("SharkX OS");
            println("A custom x86_64 operating system.");
            println("Written in C++ and Assembly.");
        }
        else if (equals(command, "version"))
        {
            println("SharkX OS v0.1.0");
        }
        else if (equals(command, "clear"))
        {
            print("\x1b[2J\x1b[H");
        }
        else if (equals(command, "halt"))
        {
            println("Shutting down CPU...");

            asm volatile (
                "cli\n"
                "hlt\n"
            );
        }
        else if (command[0] == '\0')
        {
            continue;
        }
        else
        {
            println("Unknown command. Type help.");
        }
    }
}

extern "C" void kernel_main()
{
    serial_init();

    println("");
    println("  ____  _               _    __  __");
    println(" / ___|| |__   __ _ _ __| | _|  \\/  |");
    println(" \\___ \\| '_ \\ / _` | '__| |/ / |\\/| |");
    println("  ___) | | | | (_| | |  |   <| |  | |");
    println(" |____/|_| |_|\\__,_|_|  |_|\\_\\_|  |_|");

    println("");
    println("Welcome to SharkX OS!");
    println("Kernel architecture: x86_64");
    println("Kernel language: C++");
    println("Boot protocol: Multiboot2");
    println("");

    shell();
}
