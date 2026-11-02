#pragma once

#include <stddef.h>
#include <stdarg.h>

int kvsnprintf(char* out, size_t size, const char* fmt, va_list args);
int ksnprintf(char* out, size_t size, const char* fmt, ...);
void kprintf(const char* fmt, ...);
