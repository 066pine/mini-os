#include <lib/string.h>

extern "C" {

void* memset(void* dest, int value, size_t count)
{
    unsigned char* d = (unsigned char*)dest;
    unsigned char v = (unsigned char)value;
    for (size_t i = 0; i < count; ++i)
    {
        d[i] = v;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < count; ++i)
    {
        d[i] = s[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d == s || count == 0)
    {
        return dest;
    }

    if (d < s)
    {
        for (size_t i = 0; i < count; ++i)
        {
            d[i] = s[i];
        }
    }
    else
    {
        for (size_t i = count; i > 0; --i)
        {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int memcmp(const void* a, const void* b, size_t count)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;

    for (size_t i = 0; i < count; ++i)
    {
        if (pa[i] != pb[i])
        {
            return (int)pa[i] - (int)pb[i];
        }
    }

    return 0;
}

size_t strlen(const char* s)
{
    size_t length = 0;
    while (s[length] != '\0')
    {
        ++length;
    }
    return length;
}

int strcmp(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b)
    {
        ++a;
        ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb)
        {
            return (int)ca - (int)cb;
        }
        if (ca == '\0')
        {
            return 0;
        }
    }
    return 0;
}

char* strcpy(char* dest, const char* src)
{
    char* result = dest;
    while ((*dest++ = *src++) != '\0')
    {
    }
    return result;
}

char* strncpy(char* dest, const char* src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != '\0'; ++i)
    {
        dest[i] = src[i];
    }
    for (; i < n; ++i)
    {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src)
{
    char* result = dest;
    while (*dest != '\0')
    {
        ++dest;
    }
    while ((*dest++ = *src++) != '\0')
    {
    }
    return result;
}

char* strchr(const char* s, int c)
{
    char ch = (char)c;
    while (*s != '\0')
    {
        if (*s == ch)
        {
            return (char*)s;
        }
        ++s;
    }
    return ch == '\0' ? (char*)s : nullptr;
}

char* strrchr(const char* s, int c)
{
    char ch = (char)c;
    const char* last = nullptr;
    while (*s != '\0')
    {
        if (*s == ch)
        {
            last = s;
        }
        ++s;
    }
    if (ch == '\0')
    {
        return (char*)s;
    }
    return (char*)last;
}

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

long strtol(const char* s, char** end, int base)
{
    while (is_space(*s))
    {
        ++s;
    }

    bool negative = false;
    if (*s == '+' || *s == '-')
    {
        negative = (*s == '-');
        ++s;
    }

    if (base == 0)
    {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        {
            base = 16;
            s += 2;
        }
        else if (s[0] == '0')
        {
            base = 8;
            ++s;
        }
        else
        {
            base = 10;
        }
    }

    unsigned long value = 0;
    const char* start = s;

    for (;;)
    {
        char c = *s;
        int digit;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            digit = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'Z')
        {
            digit = c - 'A' + 10;
        }
        else
        {
            break;
        }

        if (digit >= base)
        {
            break;
        }

        value = value * (unsigned long)base + (unsigned long)digit;
        ++s;
    }

    if (end != nullptr)
    {
        *end = (char*)(s == start ? start : s);
    }

    return negative ? -(long)value : (long)value;
}

int atoi(const char* s)
{
    return (int)strtol(s, nullptr, 10);
}

void trim(char* s)
{
    if (s == nullptr)
    {
        return;
    }

    char* start = s;
    while (*start != '\0' && is_space(*start))
    {
        ++start;
    }

    if (start != s)
    {
        memmove(s, start, strlen(start) + 1);
    }

    size_t length = strlen(s);
    while (length > 0 && is_space(s[length - 1]))
    {
        s[--length] = '\0';
    }
}

}
