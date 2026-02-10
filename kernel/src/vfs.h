#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_MOUNTPOINTS 10
#define MAX_OPEN_FILES 128
#define VFS_PATH_LENGTH 128
#define VFS_TYPE_LENGTH 32

// Flags for open
#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x03
#define O_CREAT  0x04

// Forward declaration
struct fs_operations;

typedef struct mountpoint {
    char path[VFS_PATH_LENGTH];
    char device[VFS_PATH_LENGTH];
    char fs_type[VFS_TYPE_LENGTH];
    struct fs_operations *operations;
    bool used;
} mountpoint_t;

typedef struct vfs_file {
    mountpoint_t *mp;       // Pointer to the mountpoint this file belongs to
    void *fs_file_data;     // Private data for the FS driver (e.g., cluster number, inode, etc.)
    uint32_t offset;        // Current file offset
    int flags;              // Open flags
    bool used;              // Is this slot used?
} vfs_file_t;

typedef struct vfs_dirent {
    uint32_t ino;
    char name[256];
} vfs_dirent_t;

struct fs_operations {
    // Open returns a void* which represents the FS-specific file handle/data
    void* (*open)(const char *path, int flags);
    void (*close)(void *fs_file);
    int (*read)(void *fs_file, void *buf, size_t size);
    int (*write)(void *fs_file, const void *buf, size_t size);
};

typedef struct fs_operations fs_operations_t;

// Public VFS API
void vfs_init();
int vfs_mount(const char *path, const char *device, const char *fs_type, fs_operations_t *ops);
int vfs_unmount(const char *path);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t size);
int vfs_write(int fd, const void *buf, size_t size);
