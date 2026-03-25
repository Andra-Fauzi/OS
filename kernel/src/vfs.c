#include "vfs.h"
#include "main.h" // for memcpy, memset, etc.
#include "util.h" // for strlen, etc.
#include "terminal.h" // for printf

// Global arrays
static mountpoint_t mountpoints[MAX_MOUNTPOINTS];
static vfs_inode_t inode_pool[MAX_INODES];
static vfs_file_desc_t file_descriptors[MAX_FILE_DESCRIPTORS];
// static vfs_file_t open_files[MAX_OPEN_FILES];

extern process_t *running_process;

static void lock_process() {
    asm volatile("cli");
}

static void unlock_process() {
    asm volatile("sti");
}

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
    lock_process();
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
    // clear descriptors
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        file_descriptors[i].inode = NULL;
        file_descriptors[i].ref_count = 0;
        file_descriptors[i].used = false;
    }
    // Clear open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        running_process->open_files[i].used = false;
        running_process->open_files[i].desc = NULL;
    }
    printf("VFS Initialized.\n");
}

static vfs_inode_t* vfs_allocate_inode(mountpoint_t *mp, void *fs_data, size_t size_of_file, uint32_t ino, uint32_t type) {
    // For now, always allocate a new one (simpler)
    for (int i = 0; i < MAX_INODES; i++) {
        if (!inode_pool[i].used) {
            inode_pool[i].used = true;
            inode_pool[i].mp = mp;
            inode_pool[i].fs_file_data = fs_data;
            inode_pool[i].ref_count = 1;
            inode_pool[i].size = size_of_file;
            inode_pool[i].ino = ino;
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

vfs_file_desc_t* vfs_allocate_descriptor(vfs_inode_t *inode, int flags) {
    // For now, always allocate a new one (simpler)
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (!file_descriptors[i].used) {
            file_descriptors[i].used = true;
            file_descriptors[i].inode = inode;
            file_descriptors[i].ref_count = 1;
            file_descriptors[i].flags = flags;
            file_descriptors[i].offset = 0;
            inode->ref_count++;
            return &file_descriptors[i];
        }
    }
    return NULL;
}

static void vfs_free_descriptor(vfs_file_desc_t *desc) {
    if (!desc) return;
    if (--desc->ref_count == 0) {
        if (desc->inode->mp && desc->inode->mp->operations && desc->inode->mp->operations->close) {
            desc->inode->mp->operations->close(desc->inode->fs_file_data);
        }
        desc->inode->used = false;
        desc->inode->mp = NULL;
        desc->inode->fs_file_data = NULL;
        desc->used = false;
        desc->flags = 0;
        desc->offset = 0;
        desc->inode = NULL;
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
        uint32_t ino = 0;
        uint32_t type = 0;
        void *fs_data = best_mp->operations->open(rel_path, flags, &size_of_file, &ino, &type);
        if (fs_data) {
            for(int i = 0; i < MAX_INODES; i++) {
                if(inode_pool[i].used && inode_pool[i].ino == ino && inode_pool[i].mp == best_mp) {
                    inode_pool[i].ref_count += 1;
                    return &inode_pool[i];
                }
            }
            vfs_inode_t *inode = vfs_allocate_inode(best_mp, fs_data, size_of_file, ino, type);
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
    lock_process();
    vfs_inode_t *inode = vfs_lookup(path, flags);
    if (!inode) {
        unlock_process();
        return -1;
    }

    vfs_file_desc_t *desc = vfs_allocate_descriptor(inode, flags);
    if (!desc) {
        unlock_process();
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!running_process->open_files[i].used) {
            running_process->open_files[i].used = true;
            running_process->open_files[i].desc = desc;
            unlock_process();
            return i; // Return FD
        }
    }
    
    printf("VFS Error: Too many open files.\n");
    vfs_free_inode(inode);
    vfs_free_descriptor(desc);
    unlock_process();
    return -1;
}

int vfs_close(int fd) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_free_descriptor(f->desc);
    
    f->used = false;
    f->desc = NULL;
    unlock_process();
    return 0;
}

int vfs_read(int fd, void *buf, size_t size) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used || !(running_process->open_files[fd].desc->flags & O_RDONLY)) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->read) {
        int bytes_read = inode->mp->operations->read(inode->fs_file_data, buf, size, f->desc->offset);
        if (bytes_read > 0) {
            f->desc->offset += bytes_read;
        }
        unlock_process();
        return bytes_read;
    }
    unlock_process();
    return -1;
}

int vfs_write(int fd, const void *buf, size_t size) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used || !(running_process->open_files[fd].desc->flags & O_WRONLY)) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->write) {
        int bytes_written = inode->mp->operations->write(inode->fs_file_data, buf, size, f->desc->offset);
        if (bytes_written > 0) {
            f->desc->offset += bytes_written;
        }
        unlock_process();
        return bytes_written;
    }
    unlock_process();
    return -1;
}

int vfs_seek(int fd, size_t offset) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    f->desc->offset = offset;
    unlock_process();
    return 0;
}

int vfs_readdir(int fd, vfs_dirent_t *dirent) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->readdir) {
        int res = inode->mp->operations->readdir(inode->fs_file_data, dirent, f->desc->offset);
        if (res == 0) {
            f->desc->offset++; // Move to next entry
            unlock_process();
            return 0;
        }
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_finddir(int fd, const char *name, vfs_dirent_t *dirent) {
    lock_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if (inode && inode->mp && inode->mp->operations && inode->mp->operations->finddir) {
        int res = inode->mp->operations->finddir(inode->fs_file_data, name, dirent);
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_mkdir(int fd, const char *name) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        int res = inode->mp->operations->mkdir(inode->fs_file_data, name);
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_rmdir(int fd, const char *name) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        int res = inode->mp->operations->rmdir(inode->fs_file_data, name);
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_rm(int fd, const char *name) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        int res = inode->mp->operations->rm(inode->fs_file_data, name);
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_create(int fd, const char *name) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    if(inode && inode->mp && inode->mp->operations && inode->mp->operations->mkdir) {
        int res = inode->mp->operations->create(inode->fs_file_data, name);
        unlock_process();
        return res;
    }
    unlock_process();
    return -1;
}

int vfs_stat(const char *path, stat_t *st) {
    lock_process();
    vfs_inode_t *inode = vfs_lookup(path, 0);
    if (!inode) {
        unlock_process();
        return -1;
    }
    st->st_ino = inode->ino;
    st->st_mode = inode->type;
    st->st_size = inode->size;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_nlink = 0;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    vfs_free_inode(inode);
    unlock_process();
    return 0;
}

int vfs_fstat(int fd, stat_t *st) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_inode_t *inode = f->desc->inode;
    st->st_ino = inode->ino;
    st->st_mode = inode->type;
    st->st_size = inode->size;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_nlink = 0;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    unlock_process();
    return 0;
}

int vfs_dup(int fd) {
    lock_process();
    if(fd < 0 || fd >= MAX_OPEN_FILES || !running_process->open_files[fd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[fd];
    vfs_file_desc_t *desc = f->desc;

    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!running_process->open_files[i].used) {
            running_process->open_files[i].used = true;
            running_process->open_files[i].desc = desc;
            desc->ref_count++;
            unlock_process();
            return i;
        }
    }
    unlock_process();
    return -1;
}

int vfs_dup2(int oldfd, int newfd) {
    lock_process();
    if(oldfd < 0 || oldfd >= MAX_OPEN_FILES || !running_process->open_files[oldfd].used) {
        unlock_process();
        return -1;
    }

    if(newfd < 0 || newfd >= MAX_OPEN_FILES || !running_process->open_files[newfd].used) {
        unlock_process();
        return -1;
    }

    vfs_file_t *f = &running_process->open_files[oldfd];
    vfs_file_desc_t *desc = f->desc;

    vfs_file_t *f2 = &running_process->open_files[newfd];
    vfs_file_desc_t *desc2 = f2->desc;

    vfs_free_descriptor(desc2);

    f2->desc = desc;
    desc->ref_count++;
    unlock_process();
    return newfd;
}

static mountpoint_t pipemp;

int vfs_pipe(int *pipefd) {
    unlock_process();
    memcpy(pipemp.fs_type, "FS_PIPE", 8);
    pipemp.operations = pipe_get_operations();
    
    vfs_inode_t *inode = vfs_allocate_inode(&pipemp, malloc(sizeof(pipe_t), 4), 0, 0, 0);
    if(!inode) {
        unlock_process();
        return -1;
    }
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!running_process->open_files[i].used) {
           pipefd[0] = i;
           vfs_file_desc_t *desc = vfs_allocate_descriptor(inode, O_RDONLY);
           if(!desc) {
                unlock_process();
                return -1;
           }
           running_process->open_files[i].used = true;
           running_process->open_files[i].desc = desc;
        }
    }
    
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!running_process->open_files[i].used) {
            pipefd[1] = i;
            vfs_file_desc_t *desc = vfs_allocate_descriptor(inode, O_WRONLY);
            if(!desc) {
                unlock_process();
                return -1;
            }
           running_process->open_files[i].used = true;
           running_process->open_files[i].desc = desc;
        }
    }

    pipe_t *pipe = (pipe_t *)inode->fs_file_data;
    pipe->read_offset = 0;
    pipe->write_offset = 0;

    unlock_process();
    return 0;
}