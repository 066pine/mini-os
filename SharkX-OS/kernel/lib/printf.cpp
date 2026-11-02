#include <lib/printf.h>
#include <lib/string.h>
#include <kernel/console.h>
#include <stdint.h>

namespace
{

struct Output
{
    char* buffer;
    size_t capacity;
    size_t position;
};

void emit(Output& out, char c)
{
    if (out.capacity > 0 && out.position + 1 < out.capacity)
    {
        out.buffer[out.position] = c;
    }
    ++out.position;
}

void emit_repeated(Output& out, char c, int count)
{
    for (int i = 0; i < count; ++i)
    {
        emit(out, c);
    }
}

void emit_string(Output& out, const char* text)
{
    if (text == nullptr)
    {
        text = "(null)";
    }
    while (*text != '\0')
    {
        emit(out, *text++);
    }
}

}

int kvsnprintf(char* out, size_t size, const char* fmt, va_list args)
{
    Output output = { out, size, 0 };

    if (size > 0)
    {
        out[0] = '\0';
    }

    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            emit(output, *fmt++);
            continue;
        }

        ++fmt;

        bool left = false;
        bool zero_pad = false;
        int width = 0;

        if (*fmt == '-')
        {
            left = true;
            ++fmt;
        }
        if (*fmt == '0')
        {
            zero_pad = true;
            ++fmt;
        }
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            ++fmt;
        }

        int length = 0;
        if (*fmt == 'l')
        {
            if (fmt[1] == 'l')
            {
                length = 2;
                fmt += 2;
            }
            else
            {
                length = 1;
                ++fmt;
            }
        }
        else if (*fmt == 'z')
        {
            length = 3;
            ++fmt;
        }

        char spec = *fmt;
        if (spec == '\0')
        {
            break;
        }
        ++fmt;

        if (spec == '%')
        {
            emit(output, '%');
            continue;
        }

        if (spec == 's')
        {
            const char* text = va_arg(args, const char*);
            if (text == nullptr)
            {
                text = "(null)";
            }
            int text_length = (int)strlen(text);
            int padding = width > text_length ? width - text_length : 0;

            if (!left)
            {
                emit_repeated(output, ' ', padding);
            }
            emit_string(output, text);
            if (left)
            {
                emit_repeated(output, ' ', padding);
            }
            continue;
        }

        if (spec == 'c')
        {
            char value = (char)va_arg(args, int);
            int padding = width > 1 ? width - 1 : 0;
            if (!left)
            {
                emit_repeated(output, ' ', padding);
            }
            emit(output, value);
            if (left)
            {
                emit_repeated(output, ' ', padding);
            }
            continue;
        }

        char sign = 0;
        const char* prefix = "";
        char digits[32];
        int digit_count = 0;
        unsigned int base = 10;
        bool uppercase = false;

        if (spec == 'd' || spec == 'i')
        {
            long long value;
            if (length == 0)
            {
                value = (long long)va_arg(args, int);
            }
            else if (length == 1)
            {
                value = (long long)va_arg(args, long);
            }
            else
            {
                value = va_arg(args, long long);
            }

            unsigned long long magnitude;
            if (value < 0)
            {
                sign = '-';
                magnitude = (unsigned long long)(-(value + 1)) + 1ULL;
            }
            else
            {
                magnitude = (unsigned long long)value;
            }

            if (magnitude == 0)
            {
                digits[digit_count++] = '0';
            }
            while (magnitude > 0)
            {
                digits[digit_count++] = (char)('0' + (magnitude % 10));
                magnitude /= 10;
            }
        }
        else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o' || spec == 'p')
        {
            unsigned long long value;
            if (spec == 'p')
            {
                value = (unsigned long long)(uintptr_t)va_arg(args, void*);
                base = 16;
                prefix = "0x";
            }
            else
            {
                if (length == 0)
                {
                    value = (unsigned long long)va_arg(args, unsigned int);
                }
                else if (length == 1)
                {
                    value = (unsigned long long)va_arg(args, unsigned long);
                }
                else
                {
                    value = va_arg(args, unsigned long long);
                }
            }

            if (spec == 'o')
            {
                base = 8;
            }
            else if (spec == 'x' || spec == 'X')
            {
                base = 16;
                uppercase = (spec == 'X');
            }

            if (value == 0)
            {
                digits[digit_count++] = '0';
            }
            while (value > 0)
            {
                unsigned int digit = (unsigned int)(value % base);
                digits[digit_count++] = (char)(digit < 10 ? ('0' + digit) : ((uppercase ? 'A' : 'a') + (digit - 10)));
                value /= base;
            }
        }
        else
        {
            emit(output, '%');
            emit(output, spec);
            continue;
        }

        int prefix_length = (int)strlen(prefix);
        int total_length = digit_count + prefix_length + (sign != 0 ? 1 : 0);
        int padding = width > total_length ? width - total_length : 0;

        if (!left)
        {
            if (zero_pad)
            {
                if (sign != 0)
                {
                    emit(output, sign);
                }
                emit_string(output, prefix);
                emit_repeated(output, '0', padding);
                for (int i = digit_count - 1; i >= 0; --i)
                {
                    emit(output, digits[i]);
                }
                continue;
            }
            emit_repeated(output, ' ', padding);
        }

        if (sign != 0)
        {
            emit(output, sign);
        }
        emit_string(output, prefix);
        for (int i = digit_count - 1; i >= 0; --i)
        {
            emit(output, digits[i]);
        }
        if (left)
        {
            emit_repeated(output, ' ', padding);
        }
    }

    if (size > 0)
    {
        size_t end = output.position < size ? output.position : size - 1;
        out[end] = '\0';
    }

    return (int)output.position;
}

int ksnprintf(char* out, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = kvsnprintf(out, size, fmt, args);
    va_end(args);
    return result;
}

void kprintf(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int length = kvsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length < 0)
    {
        return;
    }

    size_t output_length = (size_t)length;
    if (output_length >= sizeof(buffer))
    {
        output_length = sizeof(buffer) - 1;
    }

    console_write_n(buffer, output_length);
}
