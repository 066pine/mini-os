#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SHARKX_VERSION_MAJOR 1
#define SHARKX_VERSION_MINOR 0
#define SHARKX_VERSION_PATCH 0
#define SHARKX_VERSION_STRING "1.0.0"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define PAGE_SIZE 4096ULL
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;
