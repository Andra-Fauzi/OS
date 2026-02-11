#include "fat_adapter.h"
#include "memory.h"
#include "util.h"
#include "terminal.h"

typedef struct {
    char path[VFS_PATH_LENGTH];
    fat_dir_entry_t entry;
    uint32_t offset;
} fat_file_t;

void *fat_wrap_open(const char *path, int flags) {
    uint32_t cluster;
    fat_dir_entry_t *entry = get_entry_with_path(path, &cluster);
    
    // If not found, and O_CREAT is set, we should create it
    if (entry == NULL) {
        if (flags & O_CREAT) {
            fat_dir_entry_t new_entry;
            memset(&new_entry, 0, sizeof(fat_dir_entry_t));
            
            char parent_buf[VFS_PATH_LENGTH];
            char file_buf[VFS_PATH_LENGTH];
            memset(parent_buf, 0, VFS_PATH_LENGTH);
            memset(file_buf, 0, VFS_PATH_LENGTH);

            // Logic: find last slash to split directory from filename
            char *last_slash = strrchr(path, '/');
            if (last_slash) {
                int parent_len = last_slash - path;
                if (parent_len == 0) {
                    strcpy(parent_buf, "/");
                } else {
                    memcpy(parent_buf, path, parent_len);
                    parent_buf[parent_len] = 0;
                }
                strcpy(file_buf, last_slash + 1);
            } else {
                strcpy(parent_buf, "/");
                strcpy(file_buf, path);
            }
            
            // Populate Entry Name (8.3 format)
            memset(new_entry.file_name, ' ', 11);
            int len = strlen(file_buf);
            if(len > 11) len = 11;
            memcpy(new_entry.file_name, file_buf, len);
            
            create_entry(parent_buf, &new_entry);
            
            // Try getting it again
            entry = get_entry_with_path(path, &cluster);
        }
    }

    if (entry) {
        fat_file_t *file = (fat_file_t *)malloc(sizeof(fat_file_t),4);
        memset(file, 0, sizeof(fat_file_t));
        strcpy(file->path, (char*)path);
        memcpy(&file->entry, entry, sizeof(fat_dir_entry_t));
        file->offset = 0;
        free(entry); // Free the entry allocated by get_entry_with_path
        return (void*)file;
    }
    return NULL;
}

void fat_wrap_close(void *fs_file) {
    if (fs_file) {
        free(fs_file);
    }
}

int fat_wrap_read(void *fs_file, void *buf, size_t size) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file) return -1;

    // Read full file content
    uint32_t file_size = 0;
    char *data = read_data_direct_path(file->path, &file_size);
    
    if (!data) return 0; // EOF or error

    if (file->offset >= file_size) {
        free(data);
        return 0; // EOF
    }

    size_t bytes_to_read = size;
    if (file->offset + size > file_size) {
        bytes_to_read = file_size - file->offset;
    }

    memcpy(buf, data + file->offset, bytes_to_read);
    file->offset += bytes_to_read;
    
    free(data);
    return bytes_to_read;
}

int fat_wrap_write(void *fs_file, const void *buf, size_t size) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file) return -1;

    // Current FAT implementation only supports writing the full file content.
    // To support VFS write at offset, we must:
    // 1. Read existing data
    // 2. Modify buffer at offset
    // 3. Write back WHOLE data

    // 1. Read existing data
    uint32_t current_size = 0;
    char *existing = read_data_direct_path(file->path, &current_size);
    
    uint32_t new_size = current_size;
    if (file->offset + size > current_size) {
        new_size = file->offset + size;
    }

    char *new_buf = (char *)malloc(new_size, 4);
    memset(new_buf, 0, new_size);
    
    if (existing) {
        memcpy(new_buf, existing, current_size);
        free(existing);
    }
    
    // 2. Modify buffer at offset
    memcpy(new_buf + file->offset, buf, size);
    
    // 3. Write back updated content
    write_data_direct_path(file->path, new_buf, new_size);
    file->offset += size;
    
    free(new_buf);
    return size;
}

static fs_operations_t fat_ops = {
    .open = fat_wrap_open,
    .close = fat_wrap_close,
    .read = fat_wrap_read,
    .write = fat_wrap_write
};

fs_operations_t *fat_get_operations() {
    return &fat_ops;
}
