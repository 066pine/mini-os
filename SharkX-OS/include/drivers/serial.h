#pragma once

#include <stdbool.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* text);
void serial_irq_handler(void);
bool serial_has_data(void);
int serial_getc(void);
