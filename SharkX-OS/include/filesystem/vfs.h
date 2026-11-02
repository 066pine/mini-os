#pragma once

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_FILES 64
#define VFS_NAME_MAX 64
#define VFS_DATA_MAX 2048

#define VFS_PERM_EXEC 1
#define VFS_PERM_WRITE 2
#define VFS_PERM_READ 4

enum VfsResult
{
    VFS_OK = 0,
    VFS_ENOENT = -1,
    VFS_EPERM = -2,
    VFS_ENOSPC = -3,
    VFS_EEXIST = -4,
    VFS_EBADF = -5,
    VFS_ENAMETOOLONG = -6,
    VFS_EINVAL = -7
};

struct VfsFile
{
    bool used;
    char name[VFS_NAME_MAX];
    uint32_t owner;
    uint16_t mode;
    size_t size;
    uint64_t created_tick;
    uint64_t modified_tick;
    char data[VFS_DATA_MAX];
};

void vfs_init(void);

VfsFile* vfs_find(const char* name);
int vfs_create(const char* name, uint32_t owner, uint16_t mode);
int vfs_read(const char* name, char* buffer, size_t capacity, size_t* out_size, uint32_t uid);
int vfs_write(const char* name, const char* data, size_t length, uint32_t uid);
int vfs_delete(const char* name, uint32_t uid);
int vfs_chmod(const char* name, uint16_t mode, uint32_t uid);
int vfs_stat(const char* name, VfsFile* out);

bool vfs_can_access(const VfsFile* file, uint32_t uid, uint8_t permission);
int vfs_list(VfsFile* out, int max_entries);
int vfs_count(void);
const char* vfs_error_name(int result);
