#include <filesystem/vfs.h>
#include <kernel/io.h>
#include <kernel/timer.h>
#include <lib/string.h>

namespace
{
VfsFile g_files[VFS_MAX_FILES];

VfsFile* allocate_file(void)
{
    for (int i = 0; i < VFS_MAX_FILES; ++i)
    {
        if (!g_files[i].used)
        {
            return &g_files[i];
        }
    }
    return nullptr;
}

int copy_name(char* destination, const char* source)
{
    if (destination == nullptr || source == nullptr)
    {
        return VFS_EINVAL;
    }

    size_t length = strlen(source);
    if (length == 0)
    {
        return VFS_EINVAL;
    }
    if (length >= VFS_NAME_MAX)
    {
        return VFS_ENAMETOOLONG;
    }

    strcpy(destination, source);
    return VFS_OK;
}
}

void vfs_init(void)
{
    memset(g_files, 0, sizeof(g_files));

    vfs_create("/readme.txt", 0, 0644);
    vfs_create("/system/secret", 0, 0600);

    const char* readme =
        "SharkX OS RAMDisk\n"
        "This file lives entirely in kernel memory.\n"
        "Try: ls, cat /readme.txt, write /readme.txt hello, rm /readme.txt\n";

    const char* secret =
        "root-only: the launch code is SHARK-42.\n"
        "Login as guest and try to read this file to see the DAC check.\n";

    vfs_write("/readme.txt", readme, strlen(readme), 0);
    vfs_write("/system/secret", secret, strlen(secret), 0);
}

VfsFile* vfs_find(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }

    for (int i = 0; i < VFS_MAX_FILES; ++i)
    {
        if (g_files[i].used && strcmp(g_files[i].name, name) == 0)
        {
            return &g_files[i];
        }
    }
    return nullptr;
}

int vfs_create(const char* name, uint32_t owner, uint16_t mode)
{
    if (name == nullptr || name[0] == '\0')
    {
        return VFS_EINVAL;
    }

    uint64_t flags = irq_save();

    if (vfs_find(name) != nullptr)
    {
        irq_restore(flags);
        return VFS_EEXIST;
    }

    VfsFile* file = allocate_file();
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOSPC;
    }

    int result = copy_name(file->name, name);
    if (result != VFS_OK)
    {
        irq_restore(flags);
        return result;
    }

    memset(file, 0, sizeof(VfsFile));
    file->used = true;
    strcpy(file->name, name);
    file->owner = owner;
    file->mode = (uint16_t)(mode & 0777);
    file->size = 0;
    file->created_tick = timer_ticks();
    file->modified_tick = file->created_tick;

    irq_restore(flags);
    return VFS_OK;
}

bool vfs_can_access(const VfsFile* file, uint32_t uid, uint8_t permission)
{
    if (file == nullptr || !file->used)
    {
        return false;
    }

    if (uid == 0)
    {
        return true;
    }

    uint16_t bits;
    if (uid == file->owner)
    {
        bits = (uint16_t)((file->mode >> 6) & 07);
    }
    else
    {
        bits = (uint16_t)(file->mode & 07);
    }

    return (bits & permission) == permission;
}

int vfs_read(const char* name, char* buffer, size_t capacity, size_t* out_size, uint32_t uid)
{
    if (buffer == nullptr || capacity == 0)
    {
        return VFS_EINVAL;
    }

    uint64_t flags = irq_save();
    VfsFile* file = vfs_find(name);
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOENT;
    }

    if (!vfs_can_access(file, uid, VFS_PERM_READ))
    {
        irq_restore(flags);
        return VFS_EPERM;
    }

    size_t amount = file->size;
    if (amount > capacity - 1)
    {
        amount = capacity - 1;
    }

    memcpy(buffer, file->data, amount);
    buffer[amount] = '\0';

    if (out_size != nullptr)
    {
        *out_size = amount;
    }

    irq_restore(flags);
    return VFS_OK;
}

int vfs_write(const char* name, const char* data, size_t length, uint32_t uid)
{
    if (data == nullptr)
    {
        return VFS_EINVAL;
    }

    uint64_t flags = irq_save();
    VfsFile* file = vfs_find(name);
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOENT;
    }

    if (!vfs_can_access(file, uid, VFS_PERM_WRITE))
    {
        irq_restore(flags);
        return VFS_EPERM;
    }

    if (length > VFS_DATA_MAX)
    {
        irq_restore(flags);
        return VFS_ENOSPC;
    }

    memcpy(file->data, data, length);
    file->size = length;
    if (file->size < VFS_DATA_MAX)
    {
        file->data[file->size] = '\0';
    }
    file->modified_tick = timer_ticks();

    irq_restore(flags);
    return VFS_OK;
}

int vfs_delete(const char* name, uint32_t uid)
{
    uint64_t flags = irq_save();
    VfsFile* file = vfs_find(name);
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOENT;
    }

    if (!vfs_can_access(file, uid, VFS_PERM_WRITE))
    {
        irq_restore(flags);
        return VFS_EPERM;
    }

    memset(file, 0, sizeof(VfsFile));
    irq_restore(flags);
    return VFS_OK;
}

int vfs_chmod(const char* name, uint16_t mode, uint32_t uid)
{
    if (uid != 0)
    {
        return VFS_EPERM;
    }

    uint64_t flags = irq_save();
    VfsFile* file = vfs_find(name);
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOENT;
    }

    file->mode = (uint16_t)(mode & 0777);
    file->modified_tick = timer_ticks();
    irq_restore(flags);
    return VFS_OK;
}

int vfs_stat(const char* name, VfsFile* out)
{
    if (out == nullptr)
    {
        return VFS_EINVAL;
    }

    uint64_t flags = irq_save();
    VfsFile* file = vfs_find(name);
    if (file == nullptr)
    {
        irq_restore(flags);
        return VFS_ENOENT;
    }

    memcpy(out, file, sizeof(VfsFile));
    irq_restore(flags);
    return VFS_OK;
}

int vfs_list(VfsFile* out, int max_entries)
{
    if (out == nullptr || max_entries <= 0)
    {
        return 0;
    }

    int count = 0;
    uint64_t flags = irq_save();
    for (int i = 0; i < VFS_MAX_FILES && count < max_entries; ++i)
    {
        if (g_files[i].used)
        {
            memcpy(&out[count], &g_files[i], sizeof(VfsFile));
            ++count;
        }
    }
    irq_restore(flags);
    return count;
}

int vfs_count(void)
{
    int count = 0;
    uint64_t flags = irq_save();
    for (int i = 0; i < VFS_MAX_FILES; ++i)
    {
        if (g_files[i].used)
        {
            ++count;
        }
    }
    irq_restore(flags);
    return count;
}

const char* vfs_error_name(int result)
{
    switch (result)
    {
    case VFS_OK:
        return "ok";
    case VFS_ENOENT:
        return "no such file";
    case VFS_EPERM:
        return "permission denied";
    case VFS_ENOSPC:
        return "no space / too large";
    case VFS_EEXIST:
        return "file already exists";
    case VFS_EBADF:
        return "bad file descriptor";
    case VFS_ENAMETOOLONG:
        return "name too long";
    case VFS_EINVAL:
        return "invalid argument";
    default:
        return "unknown error";
    }
}
