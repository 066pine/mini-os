#include <drivers/vga.h>
#include <kernel/io.h>
#include <lib/string.h>

namespace
{
constexpr size_t VGA_WIDTH = 80;
constexpr size_t VGA_HEIGHT = 25;
constexpr uint16_t VGA_ATTRIBUTE = 0x07;
constexpr uint64_t VGA_MEMORY = 0xB8000;

uint16_t* const g_vga = (uint16_t*)VGA_MEMORY;
size_t g_row = 0;
size_t g_column = 0;

uint16_t make_entry(char c)
{
    return (uint16_t)(unsigned char)c | (uint16_t)(VGA_ATTRIBUTE << 8);
}

void update_cursor(void)
{
    uint16_t position = (uint16_t)(g_row * VGA_WIDTH + g_column);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

void scroll(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; ++row)
    {
        for (size_t column = 0; column < VGA_WIDTH; ++column)
        {
            g_vga[(row - 1) * VGA_WIDTH + column] = g_vga[row * VGA_WIDTH + column];
        }
    }

    for (size_t column = 0; column < VGA_WIDTH; ++column)
    {
        g_vga[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = make_entry(' ');
    }

    g_row = VGA_HEIGHT - 1;
}
}

void vga_init(void)
{
    vga_clear();
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
    {
        g_vga[i] = make_entry(' ');
    }
    g_row = 0;
    g_column = 0;
    update_cursor();
}

void vga_putc(char c)
{
    if (c == '\n')
    {
        g_column = 0;
        ++g_row;
    }
    else if (c == '\r')
    {
        g_column = 0;
    }
    else if (c == '\b')
    {
        if (g_column > 0)
        {
            --g_column;
        }
        else if (g_row > 0)
        {
            --g_row;
            g_column = VGA_WIDTH - 1;
        }
        g_vga[g_row * VGA_WIDTH + g_column] = make_entry(' ');
    }
    else if (c == '\t')
    {
        size_t next = (g_column / 4 + 1) * 4;
        while (g_column < next && g_column < VGA_WIDTH)
        {
            vga_putc(' ');
        }
        return;
    }
    else if ((unsigned char)c >= 32)
    {
        g_vga[g_row * VGA_WIDTH + g_column] = make_entry(c);
        ++g_column;
        if (g_column >= VGA_WIDTH)
        {
            g_column = 0;
            ++g_row;
        }
    }

    if (g_row >= VGA_HEIGHT)
    {
        scroll();
    }

    update_cursor();
}

void vga_write(const char* text)
{
    while (*text != '\0')
    {
        vga_putc(*text++);
    }
}
