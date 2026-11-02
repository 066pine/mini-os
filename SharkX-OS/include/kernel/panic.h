#pragma once

#include <stdarg.h>

[[noreturn]] void panic(const char* fmt, ...);
[[noreturn]] void panic_va(const char* fmt, va_list args);
