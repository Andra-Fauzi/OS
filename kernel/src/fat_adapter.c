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
            // Simple name extraction (assumes "/filename" or "filename")
            const char *name = path;
            if (path[0] == '/') name++;
            printf("name is %s\n",name);
            
            // Allow basic creation
            memset(new_entry.file_name, ' ', 11);
            int len = strlen((char*)name);
            if(len > 11) len = 11;
            memcpy(new_entry.file_name, name, len);
            char *path_x = strrchr(path, '/');
            if(path_x == NULL) {
                printf("kosong\n");
                printf("path is %s\n", path);
                create_entry("/", &new_entry);
                entry = get_entry_with_path(path, &cluster);
                listing_root_dir_print();
            }
            else {
                printf("ada\n");
                printf("path is %s\n", path);
                printf("index path_x is %s\n", path_x);
                int index = (int)(path_x - path);
                printf("index is %d\n", (int64_t)index);
                char *path_copy = (char*)malloc(index + 1, 4);
                memcpy(path_copy, path, index);
                path_copy[index] = '\0';
                printf("path copy %s\n", path_copy);
                create_entry(path_copy, &new_entry);
                // Try getting it again
                listing_root_dir_print();
                entry = get_entry_with_path(path, &cluster);
            }
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
