#pragma once

#include <stddef.h>
#include <stdbool.h>

extern "C" {

void* memset(void* dest, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
int memcmp(const void* a, const void* b, size_t count);

size_t strlen(const char* s);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);

long strtol(const char* s, char** end, int base);
int atoi(const char* s);
bool is_digit(char c);
bool is_space(char c);
void trim(char* s);

}
