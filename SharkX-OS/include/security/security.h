#pragma once

#include <stdint.h>

#define SECURITY_UID_ROOT 0
#define SECURITY_UID_GUEST 1

void security_init(void);
bool security_login(const char* name);
uint32_t security_current_uid(void);
const char* security_current_name(void);
const char* security_name_for_uid(uint32_t uid);
bool security_is_root(void);
