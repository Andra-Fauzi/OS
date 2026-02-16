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

#define MAX_INODES 256

// Forward declaration
struct fs_operations;

typedef struct mountpoint {
    char path[VFS_PATH_LENGTH];
    char device[VFS_PATH_LENGTH];
    char fs_type[VFS_TYPE_LENGTH];
    struct fs_operations *operations;
    bool used;
} mountpoint_t;

typedef struct vfs_inode {
    mountpoint_t *mp;       // Mountpoint this inode belongs to
    void *fs_file_data;     // FS-specific data
    uint32_t ref_count;     // Reference count (how many open files point to this)
    bool used;              // Is this slot in the pool used?
    size_t size;
} vfs_inode_t;

typedef struct vfs_file {
    vfs_inode_t *inode;     // Pointer to the underlying inode
    uint32_t offset;        // Current file offset
    int flags;              // Open flags
    bool used;              // Is this FD slot used?
} vfs_file_t;

typedef struct vfs_dirent {
    char name[VFS_PATH_LENGTH];
    uint32_t type; // 0 for file, 1 for directory
    uint32_t ino;  // Inode number (unique ID)
    size_t size;
} vfs_dirent_t;

struct fs_operations {
    // Open returns a void* which represents the FS-specific file handle/data
    void* (*open)(const char *path, int flags, size_t *size_of_file);
    void (*close)(void *fs_file);
    int (*read)(void *fs_file, void *buf, size_t size, uint32_t offset);
    int (*write)(void *fs_file, const void *buf, size_t size, uint32_t offset);
    int (*mkdir)(void *fs_file, const char *name);
    int (*create)(void *fs_file, const char *name);
    int (*rmdir)(void *fs_file, const char *name);
    int (*rm)(void *fs_file, const char *name);
    int (*readdir)(void *fs_file, vfs_dirent_t *dirent, uint32_t offset);
    int (*finddir)(void *fs_file, const char *name, vfs_dirent_t *dirent);
};

typedef struct fs_operations fs_operations_t;

// Public VFS API
void vfs_init();
int vfs_mount(const char *path, const char *device, const char *fs_type, fs_operations_t *ops);
int vfs_unmount(const char *path);
vfs_inode_t* vfs_lookup(const char *path, int flags);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t size);
int vfs_write(int fd, const void *buf, size_t size);
int vfs_readdir(int fd, vfs_dirent_t *dirent);
int vfs_finddir(int fd, const char *name, vfs_dirent_t *dirent);
int vfs_seek(int fd, size_t offset);
int vfs_mkdir(int fd, const char *name);
