#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_MOUNTPOINTS 10
#define MAX_FILE_DESCRIPTORS 128
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

typedef uint16_t umode_t;  // 16-bit type for file mode (type + permissions)

struct fs_operations;

typedef struct __attribute__((packed)) mountpoint {
    char path[VFS_PATH_LENGTH];
    char device[VFS_PATH_LENGTH];
    char fs_type[VFS_TYPE_LENGTH];
    struct fs_operations *operations;
    bool used;
} mountpoint_t;

typedef struct __attribute__((packed)) vfs_inode {
    mountpoint_t *mp;       // Mountpoint this inode belongs to
    void *fs_file_data;     // FS-specific data
    uint32_t ref_count;     // Reference count (how many files descriptors point to this)
    bool used;
    uint32_t type; // 0 for file, 1 for directory              // Is this slot in the pool used?
    size_t size;
    uint32_t ino; // inode number (unique ID) // i dont know just use the clusters in FAT or anything that unique
} vfs_inode_t;

typedef struct __attribute__((packed)) vfs_file_desc {
    vfs_inode_t *inode;     // Pointer to the underlying inode
    uint32_t offset;        // Current file offset
    int flags;              // Open flags
    uint32_t ref_count; // Reference count (how much files point to this)
    bool used;
} vfs_file_desc_t;

typedef struct __attribute__((packed)) vfs_file {
    vfs_file_desc_t *desc;
    bool used;              // Is this FD slot used?
} vfs_file_t;

typedef struct vfs_dirent {
    char name[VFS_PATH_LENGTH];
    uint32_t type; // 0 for file, 1 for directory
    uint32_t ino;  // Inode number (unique ID)
    size_t size;
} vfs_dirent_t;

typedef struct stat {
    uint64_t st_ino;
    umode_t st_mode;
    uint64_t st_size;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_nlink;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
} stat_t;

#define PIPE_BUFFER_SIZE 4096

typedef struct pipe {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    size_t read_offset;
    size_t write_offset;
} pipe_t;

struct fs_operations {
    // Open returns a void* which represents the FS-specific file handle/data
    void* (*open)(const char *path, int flags, size_t *size_of_file, uint32_t *ino, uint32_t *type);
    void (*close)(void *fs_file);
    int (*read)(void *fs_file, void *buf, size_t size, uint32_t offset);
    int (*write)(void *fs_file, const void *buf, size_t size, uint32_t offset);
    int (*mkdir)(void *fs_file, const char *name);
    int (*create)(void *fs_file, const char *name);
    int (*rmdir)(void *fs_file, const char *name);
    int (*rm)(void *fs_file, const char *name);
    int (*readdir)(void *fs_file, vfs_dirent_t *dirent, uint32_t offset);
    int (*finddir)(void *fs_file, const char *name, vfs_dirent_t *dirent);
    int (*ioctl)(void *fs_file, int request, void *arg);
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
int vfs_stat(const char *path, stat_t *st);
int vfs_fstat(int fd, stat_t *st);
int vfs_dup(int fd);
int vfs_dup2(int oldfd, int newfd);
int vfs_pipe(int *pipefd);
vfs_file_desc_t* vfs_allocate_descriptor(vfs_inode_t *inode, int flags);
int vfs_ioctl(int fd, int request, void *arg);
int vfs_fcntl(int fd, int cmd, uint64_t arg);
vfs_inode_t* vfs_allocate_inode(mountpoint_t *mp, void *fs_data, size_t size_of_file, uint32_t ino, uint32_t type);
void vfs_free_inode(vfs_inode_t *inode);

// Pipe
fs_operations_t *pipe_get_operations();
