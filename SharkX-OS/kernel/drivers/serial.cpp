#include <drivers/serial.h>
#include <kernel/io.h>
#include <stddef.h>

namespace
{
constexpr uint16_t COM1 = 0x3F8;
constexpr size_t RX_BUFFER_SIZE = 256;

char g_rx_buffer[RX_BUFFER_SIZE];
volatile uint32_t g_rx_head = 0;
volatile uint32_t g_rx_tail = 0;

bool uart_data_ready(void)
{
    return (inb(COM1 + 5) & 0x01) != 0;
}

void push_rx(char c)
{
    uint32_t next = (g_rx_head + 1) % RX_BUFFER_SIZE;
    if (next == g_rx_tail)
    {
        return;
    }
    g_rx_buffer[g_rx_head] = c;
    g_rx_head = next;
}
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);

    g_rx_head = 0;
    g_rx_tail = 0;

    while (uart_data_ready())
    {
        (void)inb(COM1);
    }

    outb(COM1 + 1, 0x01);
}

void serial_putc(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0)
    {
        cpu_pause();
    }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* text)
{
    while (*text != '\0')
    {
        serial_putc(*text++);
    }
}

void serial_irq_handler(void)
{
    while (uart_data_ready())
    {
        push_rx((char)inb(COM1));
    }
}

bool serial_has_data(void)
{
    return g_rx_head != g_rx_tail;
}

int serial_getc(void)
{
    uint64_t flags = irq_save();
    if (g_rx_head == g_rx_tail)
    {
        irq_restore(flags);
        return -1;
    }

    char c = g_rx_buffer[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1) % RX_BUFFER_SIZE;
    irq_restore(flags);
    return (int)(unsigned char)c;
}
