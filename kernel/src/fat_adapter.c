#include "fat_adapter.h"
#include "memory.h"
#include "util.h"
#include "terminal.h"

typedef struct {
    char path[VFS_PATH_LENGTH];
    fat_dir_entry_t entry;
    uint32_t cluster;
    bool isroot;
    bool isdir;
} fat_file_t;

static void fat_translate(const char *name, char new_name[12]) {
    memset(new_name, ' ', 11);
    char *dot = strchr(name, '.');
    int name_len = dot ? (dot - name) : strlen(name);
    if (name_len > 8) name_len = 8;
    memcpy(new_name, name, name_len);
    if (dot) {
        int ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(new_name + 8, dot + 1, ext_len);
    }
    // Uppercase it? FAT is case-insensitive, entry is usually uppercase
    for(int i=0; i<11; i++) {
        if(new_name[i] >= 'a' && new_name[i] <= 'z') new_name[i] -= 32;
    }
    new_name[11] = '\0';
}

void *fat_wrap_open(const char *path, int flags, size_t *size_of_file, uint32_t *ino, uint32_t *type) {
    
    *size_of_file = 0;
    uint32_t cluster = -1;
    fat_dir_entry_t *entry = get_entry_with_path(path, &cluster);

    if(entry == NULL && cluster != -1 && !(flags & O_CREAT)) {
        fat_file_t *file = (fat_file_t *)malloc(sizeof(fat_file_t),4);
        memset(file, 0, sizeof(fat_file_t));
        strcpy(file->path, (char*)path);
        file->isroot = true;
        file->isdir = true;
        file->cluster = cluster;
        *ino = cluster;
        *type = 1;
        return (void*)file;
    }

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
            char filename[12];
            memset(filename, ' ', 12);
            memcpy(&filename, &new_entry.file_name, 11);
            fat_translate(file_buf, filename);
            printf("filename is %s\n", filename);
            printf("file_buf is %s\n", file_buf);
            memcpy(new_entry.file_name, filename, 11);
            
            create_entry(parent_buf, &new_entry);
            
            // Try getting it again
            printf("path is %s\n", path);
            entry = get_entry_with_path(path, &cluster);
        }
    }

    if (entry) {
        fat_file_t *file = (fat_file_t *)malloc(sizeof(fat_file_t),4);
        memset(file, 0, sizeof(fat_file_t));
        strcpy(file->path, (char*)path);
        memcpy(&file->entry, entry, sizeof(fat_dir_entry_t));
        file->isroot = false;
        if(entry->attribute_file == 0x10) {
            file->isdir = true;
            *type = 1;
        } else {
            file->isdir = false;
            *type = 0;
        }
        file->cluster = cluster;
        *size_of_file = entry->size_file;
        *ino = cluster;

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

int fat_wrap_read(void *fs_file, void *buf, size_t size, uint32_t offset) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file) return -1;

    if(file->isdir || file->isroot) {
        printf("THIS IS DIRECTORY\n");
        return -1;
    }

    // Read full file content
    uint32_t file_size = 0;
    char *data = read_data_direct_path(file->path, &file_size);
    
    if (!data) return 0; // EOF or error

    if (offset >= file_size) {
        free(data);
        return 0; // EOF
    }

    size_t bytes_to_read = size;
    if (offset + size > file_size) {
        bytes_to_read = file_size - offset;
    }

    memcpy(buf, data + offset, bytes_to_read);
    
    free(data);
    return bytes_to_read;
}

int fat_wrap_write(void *fs_file, const void *buf, size_t size, uint32_t offset) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file) return -1;

    if(file->isdir || file->isroot) {
        printf("THIS IS DIRECTORY\n");
        return -1;
    }

    // Current FAT implementation only supports writing the full file content.
    // To support VFS write at offset, we must:
    // 1. Read existing data
    // 2. Modify buffer at offset
    // 3. Write back WHOLE data

    // 1. Read existing data
    uint32_t current_size = 0;
    char *existing = read_data_direct_path(file->path, &current_size);
    
    uint32_t new_size = current_size;
    if (offset + size > current_size) {
        new_size = offset + size;
    }

    char *new_buf = (char *)malloc(new_size, 4);
    memset(new_buf, 0, new_size);
    
    if (existing) {
        memcpy(new_buf, existing, current_size);
        free(existing);
    }
    
    // 2. Modify buffer at offset
    memcpy(new_buf + offset, buf, size);
    
    // 3. Write back updated content
    write_data_direct_path(file->path, new_buf, new_size);
    
    free(new_buf);
    return size;
}

typedef struct {
    uint32_t target_idx;
    uint32_t current_idx;
    bool found;
    fat_dir_entry_t entry;
} readdir_ctx_t;

static bool fat_readdir_cb(fat_dir_entry_t *entry, uint32_t sector_lba, uint32_t entry_idx, void *p) {
    (void)sector_lba; (void)entry_idx;
    readdir_ctx_t *ctx = (readdir_ctx_t*)p;
    
    if (entry->file_name[0] == 0) return true; // End of directory
    if (entry->file_name[0] == 0xE5) return false; // Deleted
    if ((entry->attribute_file & 0x0F) == 0x0F) return false; // LFN
    
    if (ctx->current_idx == ctx->target_idx) {
        ctx->entry = *entry;
        ctx->found = true;
        return true; // Stop
    }
    
    ctx->current_idx++;
    return false;
}

int fat_wrap_readdir(void *fs_file, vfs_dirent_t *dirent, uint32_t offset) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file || !file->isdir) return -1;

    readdir_ctx_t ctx;
    ctx.target_idx = offset;
    ctx.current_idx = 0;
    ctx.found = false;

    fat_foreach_entry(file->cluster, fat_readdir_cb, &ctx);

    if (ctx.found) {
        // Convert 8.3 name to string
        int name_len = 0;
        for (int i = 0; i < 8; i++) {
            if (ctx.entry.file_name[i] == ' ') break;
            dirent->name[name_len++] = ctx.entry.file_name[i];
        }
        if (ctx.entry.file_name[8] != ' ') {
            dirent->name[name_len++] = '.';
            for (int i = 8; i < 11; i++) {
                if (ctx.entry.file_name[i] == ' ') break;
                dirent->name[name_len++] = ctx.entry.file_name[i];
            }
        }
        dirent->name[name_len] = '\0';
        dirent->type = (ctx.entry.attribute_file & 0x10) ? 1 : 0;
        dirent->ino = (ctx.entry.first_cluster_high << 16) | ctx.entry.first_cluster_low;
        dirent->size = ctx.entry.size_file;
        return 0;
    }

    return -1; // Not found (EOF)
}

int fat_wrap_finddir(void *fs_file, const char *name, vfs_dirent_t *dirent) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if (!file || !file->isdir) return -1;

    // Convert name to 8.3
    char name_11[11];
    memset(name_11, ' ', 11);
    char *dot = strchr(name, '.');
    int name_len = dot ? (dot - name) : strlen(name);
    if (name_len > 8) name_len = 8;
    memcpy(name_11, name, name_len);
    if (dot) {
        int ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(name_11 + 8, dot + 1, ext_len);
    }
    // Uppercase it? FAT is case-insensitive, entry is usually uppercase
    for(int i=0; i<11; i++) {
        if(name_11[i] >= 'a' && name_11[i] <= 'z') name_11[i] -= 32;
    }

    fat_dir_entry_t entry;
    if (fat_find_entry_in_dir(file->cluster, name_11, &entry)) {
        strcpy(dirent->name, (char*)name); // Return the requested name or formatted one?
        dirent->type = (entry.attribute_file & 0x10) ? 1 : 0;
        dirent->ino = (entry.first_cluster_high << 16) | entry.first_cluster_low;
        return 0;
    }

    return -1;
}

int fat_wrap_mkdir(void *fs_file, const char *name) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if(!file->isdir) {
        return -1;
    }
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(fat_dir_entry_t));
    memset(&entry.file_name, ' ', 11);
    memcpy(&entry.file_name, name, strlen(name));
    entry.attribute_file = 0x10;
    create_entry(file->path, &entry);
    return 0;
}

int fat_wrap_create(void *fs_file, const char *name) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if(!file->isdir) {
        return -1;
    }
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(fat_dir_entry_t));
    memset(&entry.file_name, ' ', 11);
    memcpy(&entry.file_name, name, strlen(name));
    create_entry(file->path, &entry);
    return 0;
}

int fat_wrap_rm(void *fs_file, const char *name) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if(!file->isdir) {
        return -1;
    }
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(fat_dir_entry_t));
    memset(&entry.file_name, ' ', 11);
    memcpy(&entry.file_name, name, strlen(name));
    delete_entry(file->path, &entry);
    return 0;
}

int fat_wrap_rmdir(void *fs_file, const char *name) {
    fat_file_t *file = (fat_file_t *)fs_file;
    if(!file->isdir) {
        return -1;
    }
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(fat_dir_entry_t));
    memset(&entry.file_name, ' ', 11);
    memcpy(&entry.file_name, name, strlen(name));
    delete_entry(file->path, &entry);
    return 0;
}

int fat_wrap_ioctl(void *fs_file, int request, void *arg) {

}

static fs_operations_t fat_ops = {
    .open = fat_wrap_open,
    .close = fat_wrap_close,
    .read = fat_wrap_read,
    .write = fat_wrap_write,
    .readdir = fat_wrap_readdir,
    .finddir = fat_wrap_finddir,
    .mkdir = fat_wrap_mkdir,
    .create = fat_wrap_create,
    .rm = fat_wrap_rm,
    .rmdir = fat_wrap_rmdir,
    .ioctl = fat_wrap_ioctl
};

fs_operations_t *fat_get_operations() {
    return &fat_ops;
}
