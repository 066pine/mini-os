#include <security/security.h>
#include <lib/string.h>

namespace
{
struct UserEntry
{
    uint32_t uid;
    const char* name;
};

const UserEntry g_users[] = {
    { SECURITY_UID_ROOT, "root" },
    { SECURITY_UID_GUEST, "guest" }
};

uint32_t g_current_uid = SECURITY_UID_ROOT;
}

void security_init(void)
{
    g_current_uid = SECURITY_UID_ROOT;
}

bool security_login(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }

    for (size_t i = 0; i < sizeof(g_users) / sizeof(g_users[0]); ++i)
    {
        if (strcmp(name, g_users[i].name) == 0)
        {
            g_current_uid = g_users[i].uid;
            return true;
        }
    }

    return false;
}

uint32_t security_current_uid(void)
{
    return g_current_uid;
}

const char* security_current_name(void)
{
    return security_name_for_uid(g_current_uid);
}

const char* security_name_for_uid(uint32_t uid)
{
    for (size_t i = 0; i < sizeof(g_users) / sizeof(g_users[0]); ++i)
    {
        if (g_users[i].uid == uid)
        {
            return g_users[i].name;
        }
    }
    return "unknown";
}

bool security_is_root(void)
{
    return g_current_uid == SECURITY_UID_ROOT;
}
