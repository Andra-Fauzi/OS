#include "vfs.h"
#include "main.h" // for memcpy, memset, etc.
#include "util.h" // for strlen, etc.
#include "terminal.h" // for printf

// Global arrays
static mountpoint_t mountpoints[MAX_MOUNTPOINTS];
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
    // We cast src to char* because util.h's strcpy might not take const char*
    // based on previous analysis of util.h
    strcpy(dest, (char*)src);
}

void vfs_init() {
    printf("Initializing VFS...\n");
    // Clear mountpoints
    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        mountpoints[i].used = false;
        memset(mountpoints[i].path, 0, VFS_PATH_LENGTH);
    }
    // Clear open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_files[i].used = false;
        open_files[i].mp = NULL;
        open_files[i].fs_file_data = NULL;
    }
    printf("VFS Initialized.\n");
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

int vfs_open(const char *path, int flags) {
    // 1. Find best matching mountpoint
    mountpoint_t *best_mp = NULL;
    int best_len = 0;

    for (int i = 0; i < MAX_MOUNTPOINTS; i++) {
        if (!mountpoints[i].used) continue;
        
        int len = strlen((char*)mountpoints[i].path);
        if (vfs_strncmp(path, mountpoints[i].path, len) == 0) {
             // Basic match check (should be more robust for subdirs, but sufficient for simple VFS)
             if (len > best_len) {
                 best_mp = &mountpoints[i];
                 best_len = len;
             }
        }
    }

    if (best_mp == NULL) {
        printf("VFS Error: No mountpoint found for path %s\n", path);
        return -1;
    }

    // 2. Determine relative path
    const char *rel_path = path + best_len;
    if (*rel_path == '\0') {
        // Only if the path matches mountpoint exactly and it's root or something?
        // Usually should be "/" if it was just mounted at root. 
        // But if mounted at "/mnt", path "/mnt" -> rel_path "" -> should be "/"
        // Let's assume FS handles "" or "/" correctly or we force "/"
        if (best_len > 0 && best_mp->path[best_len-1] != '/' && *rel_path == '\0') {
             rel_path = "/";
        }
    }
    
    // 3. Call FS open
    if (best_mp->operations && best_mp->operations->open) {
        void *fs_file = best_mp->operations->open(rel_path, flags);
        if (fs_file) {
            // 4. Find free file slot
            for (int i = 0; i < MAX_OPEN_FILES; i++) {
                if (!open_files[i].used) {
                    open_files[i].used = true;
                    open_files[i].mp = best_mp;
                    open_files[i].fs_file_data = fs_file;
                    open_files[i].flags = flags;
                    open_files[i].offset = 0; 
                    return i; // Return FD
                }
            }
            // No free slots
            printf("VFS Error: Too many open files.\n");
            // Should close underlying fs_file? FS dependent.
            if(best_mp->operations->close) best_mp->operations->close(fs_file);
            return -1;
        }
    }
    
    return -1; // Failed to open
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    if (f->mp && f->mp->operations && f->mp->operations->close) {
        f->mp->operations->close(f->fs_file_data);
    }
    
    f->used = false;
    f->mp = NULL;
    f->fs_file_data = NULL;
    return 0;
}

int vfs_read(int fd, void *buf, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    if (f->mp && f->mp->operations && f->mp->operations->read) {
        int bytes_read = f->mp->operations->read(f->fs_file_data, buf, size);
        if (bytes_read > 0) {
            f->offset += bytes_read;
        }
        return bytes_read;
    }
    return -1;
}

int vfs_write(int fd, const void *buf, size_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].used) {
        return -1;
    }

    vfs_file_t *f = &open_files[fd];
    if (f->mp && f->mp->operations && f->mp->operations->write) {
        int bytes_written = f->mp->operations->write(f->fs_file_data, buf, size);
        if (bytes_written > 0) {
            f->offset += bytes_written;
        }
        return bytes_written;
    }
    return -1;
}
