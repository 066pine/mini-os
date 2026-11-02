#pragma once

#include <stddef.h>

void console_init(void);
void console_putc(char c);
void console_write(const char* text);
void console_write_n(const char* text, size_t length);
void console_clear(void);

int console_getchar(void);
