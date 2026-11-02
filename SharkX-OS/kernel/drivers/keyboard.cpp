#include <drivers/keyboard.h>
#include <kernel/io.h>
#include <lib/string.h>

namespace
{
constexpr size_t KEY_BUFFER_SIZE = 256;

char g_buffer[KEY_BUFFER_SIZE];
volatile uint32_t g_head = 0;
volatile uint32_t g_tail = 0;
bool g_shift = false;
bool g_extended = false;

const char g_normal_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b',  'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

const char g_shift_map[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B',  'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

void push(char c)
{
    uint32_t next = (g_head + 1) % KEY_BUFFER_SIZE;
    if (next == g_tail)
    {
        return;
    }
    g_buffer[g_head] = c;
    g_head = next;
}
}

void keyboard_init(void)
{
    g_head = 0;
    g_tail = 0;
    g_shift = false;
    g_extended = false;

    while ((inb(0x64) & 0x01) != 0)
    {
        (void)inb(0x60);
    }
}

void keyboard_irq_handler(void)
{
    uint8_t status = inb(0x64);
    if ((status & 0x01) == 0)
    {
        return;
    }

    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0)
    {
        g_extended = true;
        return;
    }

    if (g_extended)
    {
        g_extended = false;
        return;
    }

    if (scancode == 0x2A || scancode == 0x36)
    {
        g_shift = true;
        return;
    }

    if (scancode == 0xAA || scancode == 0xB6)
    {
        g_shift = false;
        return;
    }

    if ((scancode & 0x80) != 0)
    {
        return;
    }

    char c = g_shift ? g_shift_map[scancode] : g_normal_map[scancode];
    if (c != 0)
    {
        push(c);
    }
}

int keyboard_pop(void)
{
    uint64_t flags = irq_save();
    if (g_tail == g_head)
    {
        irq_restore(flags);
        return -1;
    }

    char c = g_buffer[g_tail];
    g_tail = (g_tail + 1) % KEY_BUFFER_SIZE;
    irq_restore(flags);
    return (int)(unsigned char)c;
}
