#include "vfs.h"
#include "main.h" // for memcpy, memset, etc.
#include "util.h" // for strlen, etc.
#include "terminal.h" // for printf

// Global arrays
static mountpoint_t mountpoints[MAX_MOUNTPOINTS];
static vfs_inode_t inode_pool[MAX_INODES];
static vfs_file_t open_files[MAX_OPEN_FILES];

// Helper: Custom strncmp since it might not be in util.h or main.h
static int vfs_strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        if (*s1 != *s2) return (*(unsigned char *)s1 - *(unsigned char *)s2);
        if (*s1 == '\0') return 0;
        s1++;
        s2++;
        n--;
    }
    return 0;
}

// Helper: Custom strcpy that handles const char* source cast
static void vfs_strcpy(char *dest, const char *src) {
    strcpy(dest, (char*)src);
}

void vfs_init() {
    printf("Initializing VFS...\n");
    // Clear mountpoints
    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        mountpoints[i].used = false;
        memset(mountpoints[i].path, 0, VFS_PATH_LENGTH);
    }
    // Clear inode pool
    for (int i = 0; i < MAX_INODES; i++) {
        inode_pool[i].used = false;
        inode_pool[i].mp = NULL;
        inode_pool[i].fs_file_data = NULL;
        inode_pool[i].ref_count = 0;
    }
    // Clear open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_files[i].used = false;
        open_files[i].inode = NULL;
    }
    printf("VFS Initialized.\n");
}

static vfs_inode_t* vfs_allocate_inode(mountpoint_t *mp, void *fs_data, size_t size_of_file) {
    // For now, always allocate a new one (simpler)
    for (int i = 0; i < MAX_INODES; i++) {
        if (!inode_pool[i].used) {
            inode_pool[i].used = true;
            inode_pool[i].mp = mp;
            inode_pool[i].fs_file_data = fs_data;
            inode_pool[i].ref_count = 1;
            inode_pool[i].size = size_of_file;
            return &inode_pool[i];
        }
    }
    return NULL;
}

static void vfs_free_inode(vfs_inode_t *inode) {
    if (!inode) return;
    if (--inode->ref_count == 0) {
        if (inode->mp && inode->mp->operations && inode->mp->operations->close) {
            inode->mp->operations->close(inode->fs_file_data);
        }
        inode->used = false;
        inode->mp = NULL;
        inode->fs_file_data = NULL;
    }
}

int vfs_mount(const char *path, const char *device, const char *fs_type, fs_operations_t *ops) {
    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        if (!mountpoints[i].used) {
            vfs_strcpy(mountpoints[i].path, path);
            vfs_strcpy(mountpoints[i].device, device);
            vfs_strcpy(mountpoints[i].fs_type, fs_type);
            mountpoints[i].operations = ops;
            mountpoints[i].used = true;
            printf("Mounted %s to %s with type %s\n", device, path, fs_type);
            return 0; // Success
        }
    }
    printf("VFS Error: No free mount slots.\n");
    return -1; // Fail
}

int vfs_unmount(const char *path) {
    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        if (mountpoints[i].used && vfs_strncmp(mountpoints[i].path, path, VFS_PATH_LENGTH) == 0) {
            mountpoints[i].used = false;
            printf("Unmounted %s\n", path);
            return 0; // Success
        }
    }
    return -1; // Not found
}

vfs_inode_t* vfs_lookup(const char *path, int flags) {
    mountpoint_t *best_mp = NULL;
    int best_len = 0;

    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        if (!mountpoints[i].used) continue;
        int len = strlen((char*)mountpoints[i].path);
        if (vfs_strncmp(path, mountpoints[i].path, len) == 0) {
             if (len > best_len) {
                 best_mp = &mountpoints[i];
                 best_len = len;
             }
        }
    }

    if (best_mp == NULL) {
        printf("VFS Error: No mountpoint found for path %s\n", path);
        return NULL;
    }

    const char *rel_path = path + best_len;
    if (best_len > 0 && best_mp->path[best_len-1] != '/' && *rel_path == '\0') {
         rel_path = "/";
    }
    
    if (best_mp->operations && best_mp->operations->open) {
        size_t size_of_file = 0;
        void *fs_data = best_mp->operations->open(rel_path, flags, &size_of_file);
        if (fs_data) {
            vfs_inode_t *inode = vfs_allocate_inode(best_mp, fs_data, size_of_file);
            if (!inode) {
                printf("VFS Error: Out of inodes.\n");
                best_mp->operations->close(fs_data);
                return NULL;
            }
            return inode;
        }
    }
    return NULL;
}

int vfs_open(const char *path, int flags) {
    vfs_inode_t *inode = vfs_lookup(path, flags);
    if (!inode) {
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used = true;
            open_files[i].inode = inode;
            open_files[i].flags = flags;
            open_files[i].offset = 0; 
            return i; // Return FD
        }
    }

    printf("VFS Error: Too many open files.\n");
    vfs_free_inode(inode);
    return -1;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_free_inode(f->inode);
    
    f->used = false;
    f->inode = NULL;
    f->offset = 0;
    return 0;
}

int vfs_read(int fd, void *buf, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used || !(open_files[fd].flags & O_RDONLY)) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->read) {
        int bytes_read = inode->mp->operations->read(inode->fs_file_data, buf, size, f->offset);
        if (bytes_read > 0) {
            f->offset += bytes_read;
        }
        return bytes_read;
    }
    return -1;
}

int vfs_write(int fd, const void *buf, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used || !(open_files[fd].flags & O_WRONLY)) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->write) {
        int bytes_written = inode->mp->operations->write(inode->fs_file_data, buf, size, f->offset);
        if (bytes_written > 0) {
            f->offset += bytes_written;
        }
        return bytes_written;
    }
    return -1;
}

int vfs_seek(int fd, size_t offset) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    f->offset = offset;
    return 0;
}

int vfs_readdir(int fd, vfs_dirent_t *dirent) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->readdir) {
        int res = inode->mp->operations->readdir(inode->fs_file_data, dirent, f->offset);
        if (res == 0) {
            f->offset++; // Move to next entry
            return 0;
        }
        return res;
    }
    return -1;
}

int vfs_finddir(int fd, const char *name, vfs_dirent_t *dirent) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->finddir) {
        return inode->mp->operations->finddir(inode->fs_file_data, name, dirent);
    }
    return -1;
}

int vfs_mkdir(int fd, const char *name) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        return inode->mp->operations->mkdir(inode->fs_file_data, name);
    }
    return -1;
}

int vfs_rmdir(int fd, const char *name) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        return inode->mp->operations->rmdir(inode->fs_file_data, name);
    }
    return -1;
}

int vfs_rm(int fd, const char *name) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        return inode->mp->operations->rm(inode->fs_file_data, name);
    }
    return -1;
}

int vfs_create(int fd, const char *name) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    vfs_inode_t *inode = f->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        return inode->mp->operations->create(inode->fs_file_data, name);
    }
    return -1;
}
