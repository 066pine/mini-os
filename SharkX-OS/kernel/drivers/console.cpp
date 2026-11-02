#include <kernel/console.h>
#include <kernel/io.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>

void console_init(void)
{
    serial_init();
    vga_init();
}

void console_putc(char c)
{
    console_write_n(&c, 1);
}

void console_write(const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    size_t length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    console_write_n(text, length);
}

void console_write_n(const char* text, size_t length)
{
    if (text == nullptr || length == 0)
    {
        return;
    }

    uint64_t flags = irq_save();
    for (size_t i = 0; i < length; ++i)
    {
        char c = text[i];
        if (c == '\n')
        {
            serial_putc('\r');
            serial_putc('\n');
            vga_putc('\n');
        }
        else
        {
            serial_putc(c);
            vga_putc(c);
        }
    }
    irq_restore(flags);
}

void console_clear(void)
{
    static const char ansi_clear[] = "\x1b[2J\x1b[H";
    uint64_t flags = irq_save();
    for (size_t i = 0; i < sizeof(ansi_clear) - 1; ++i)
    {
        serial_putc(ansi_clear[i]);
    }
    vga_clear();
    irq_restore(flags);
}

int console_getchar(void)
{
    int c = keyboard_pop();
    if (c >= 0)
    {
        return c;
    }

    if (serial_has_data())
    {
        return serial_getc();
    }

    return -1;
}
