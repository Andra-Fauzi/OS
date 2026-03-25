#include "vfs.h"
#include "util.h"

void *pipe_open(const char *path, int flags, size_t *size_of_file, uint32_t *ino, uint32_t *type) {
    
}

void pipe_close(void *fs_file) {
    
}

int pipe_read(void *fs_file, void *buf, size_t size, uint32_t offset) {
    pipe_t *pipe = (pipe_t*)fs_file;
    if(offset + size > PIPE_BUFFER_SIZE) {
        return -1;
    }
    memcpy(buf, pipe->buffer + offset, size);
    pipe->read_offset += size;
    return size;
}

int pipe_write(void *fs_file, const void *buf, size_t size, uint32_t offset) {
    pipe_t *pipe = (pipe_t*)fs_file;
    if(offset + size > PIPE_BUFFER_SIZE) {
        return -1;
    }
    memcpy(pipe->buffer + offset, buf, size);
    pipe->write_offset += size;
    return size;
}

int pipe_mkdir(void *fs_file, const char *name) {
    
}

int pipe_create(void *fs_file, const char *name) {
    
}

int pipe_rmdir(void *fs_file, const char *name) {
    
}

int pipe_rm(void *fs_file, const char *name) {
    
}

int pipe_readdir(void *fs_file, vfs_dirent_t *dirent, uint32_t offset) {
    
}

int pipe_finddir(void *fs_file, const char *name, vfs_dirent_t *dirent) {
    
}

static fs_operations_t pipe_ops = {
    .open = pipe_open,
    .close = pipe_close,
    .read = pipe_read,
    .write = pipe_write,
    .mkdir = pipe_mkdir,
    .create = pipe_create,
    .rmdir = pipe_rmdir,
    .rm = pipe_rm,
    .readdir = pipe_readdir,
    .finddir = pipe_finddir
};

fs_operations_t *pipe_get_operations() {
    return &pipe_ops;
}