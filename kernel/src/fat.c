#include "fat.h"
#include "util.h"
#include "memory.h"

// --- Constants ---
#define FAT_SECTOR_SIZE 512
#define FAT_ENTRY_SIZE 32
#define FAT32_MASK 0x0FFFFFFF
#define FAT32_EOF 0x0FFFFFF8
#define FAT16_EOF 0xFFF8
#define FAT_FREE 0x00000000
#define FAT_DELETED 0xE5

// --- Global State ---
uint32_t OFFSET_FAT = 0;
enum FatType fat_type;

static uint8_t fat_bpb_buffer[FAT_SECTOR_SIZE];
fat_BS_t *fat = (fat_BS_t *)fat_bpb_buffer;
fat_extBS_16_t *fat_16 = NULL;
fat_extBS_32_t *fat_32 = NULL;

uint32_t total_sectors = 0;
uint32_t fat_size = 0;
uint32_t root_dir_sectors = 0;
uint32_t first_data_sector = 0;
uint32_t first_fat_sector = 0; 
uint32_t data_sector = 0;
uint32_t total_clusters = 0;
uint32_t first_root_dir_sector = 0; // For FAT16
uint32_t root_cluster_32 = 0;       // For FAT32

// --- Helper Prototypes ---
uint32_t fat_cluster_to_lba(uint32_t cluster);
uint32_t fat_read_fat_entry(uint32_t cluster);
void fat_write_fat_entry(uint32_t cluster, uint32_t value);
uint32_t fat_alloc_cluster();

// --- Low Level Helpers ---

uint32_t fat_cluster_to_lba(uint32_t cluster) {
    return OFFSET_FAT + ((cluster - 2) * fat->sectors_per_cluster) + first_data_sector;
}

uint32_t fat_read_fat_entry(uint32_t cluster) {
    uint32_t fat_sector = first_fat_sector;
    uint32_t fat_offset = 0;
    uint32_t val = 0;
    uint8_t *buf = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
    
    if (fat_type == FAT32) {
        fat_offset = cluster * 4;
        fat_sector += (fat_offset / FAT_SECTOR_SIZE);
        uint32_t ent_offset = fat_offset % FAT_SECTOR_SIZE;
        ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf);
        val = *(uint32_t*)&buf[ent_offset] & FAT32_MASK;
    } else {
        fat_offset = cluster * 2;
        fat_sector += (fat_offset / FAT_SECTOR_SIZE);
        uint32_t ent_offset = fat_offset % FAT_SECTOR_SIZE;
        ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf);
        val = *(uint16_t*)&buf[ent_offset];
    }
    free(buf);
    return val;
}

void fat_write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_sector = first_fat_sector;
    uint32_t fat_offset = 0;
    uint8_t *buf = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);

    if (fat_type == FAT32) {
        fat_offset = cluster * 4;
        fat_sector += (fat_offset / FAT_SECTOR_SIZE);
        uint32_t ent_offset = fat_offset % FAT_SECTOR_SIZE;
        ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf); // Read-Modify-Write
        *(uint32_t*)&buf[ent_offset] = (value & FAT32_MASK);
        ahci_write(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf);
    } else {
        fat_offset = cluster * 2;
        fat_sector += (fat_offset / FAT_SECTOR_SIZE);
        uint32_t ent_offset = fat_offset % FAT_SECTOR_SIZE;
        ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf);
        *(uint16_t*)&buf[ent_offset] = (uint16_t)value;
        ahci_write(sataport, OFFSET_FAT + fat_sector, 0, 1, (uint16_t*)buf);
    }
    free(buf);
}

uint32_t fat_alloc_cluster() {
    // Naively search for free cluster (0x00) starting from 2
    uint32_t cluster = 2;
    uint32_t max_cluster = total_clusters + 2;
    while(cluster < max_cluster) {
        if(fat_read_fat_entry(cluster) == FAT_FREE) {
            uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
            fat_write_fat_entry(cluster, 0x0FFFFFFF); // Mark allocated immediately (EOC)
            return cluster;
        }
        cluster++;
    }
    return 0; // Full
}

// Compatibility Wrappers needed for older code if any
uint16_t FAT16_read(uint16_t c) { return (uint16_t)fat_read_fat_entry(c); }
uint32_t FAT32_read(uint32_t c) { return fat_read_fat_entry(c); }
void FAT16_write(uint16_t c, uint16_t v) { fat_write_fat_entry(c, v); }
void FAT32_write(uint32_t c, uint32_t v) { fat_write_fat_entry(c, v); }

// --- Initialization ---

void fat_init() {
    uint16_t *buf = (uint16_t *)malloc(FAT_SECTOR_SIZE, 4);
    memset(buf, 0, FAT_SECTOR_SIZE);

    if(!ahci_read(sataport, 0, 0, 1, buf)) {
        printf("AHCI Read Failed\n"); free(buf); return;
    }

    MBR_t *mbr = (MBR_t*)buf;
    partition_entry_t *partition = (partition_entry_t*)mbr->partition_table;
    for(int i=0; i<4; i++) {
        if(partition[i].type == 0xEF) {
            OFFSET_FAT = partition[i].LBA_start_sector; break;
        }
    }
    
    memset(buf, 0, FAT_SECTOR_SIZE);
    if(ahci_read(sataport, OFFSET_FAT, 0, 1, buf)) {
        memcpy(fat, buf, FAT_SECTOR_SIZE);
        if(fat->table_size_16 == 0) {
            fat_type = FAT32;
            fat_32 = (fat_extBS_32_t *)fat->extended_section;
            total_sectors = fat->total_sectors_32;
            fat_size = fat_32->table_size_32;
            first_fat_sector = fat->reserved_sector_count;
            first_data_sector = fat->reserved_sector_count + (fat->table_count * fat_size);
            root_cluster_32 = fat_32->root_cluster;
            total_clusters = (total_sectors - first_data_sector) / fat->sectors_per_cluster;
            printf("FAT32 Initialized: %d clusters\n", total_clusters);
        } else {
            fat_type = FAT16;
            fat_16 = (fat_extBS_16_t *)fat->extended_section;
            total_sectors = fat->total_sectors_16;
            fat_size = fat->table_size_16;
            root_dir_sectors = ((fat->root_entry_count * 32) + (FAT_SECTOR_SIZE - 1)) / FAT_SECTOR_SIZE;
            first_fat_sector = fat->reserved_sector_count;
            first_data_sector = fat->reserved_sector_count + (fat->table_count * fat_size) + root_dir_sectors;
            data_sector = total_sectors - first_data_sector;
            total_clusters = data_sector / fat->sectors_per_cluster;
            first_root_dir_sector = first_data_sector - root_dir_sectors;
            printf("FAT16 Init. Size: %d sectors.\n", total_sectors);
        }
    } else {
        printf("AHCI can't read boot sector\n");
    }
    free(buf);
}


void fat_foreach_entry(uint32_t start_cluster, dir_iter_cb cb, void *ctx) {
    uint8_t *buf = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
    uint32_t entries_per_sector = FAT_SECTOR_SIZE / FAT_ENTRY_SIZE;

    if (fat_type == FAT16 && start_cluster == 0) {
        for(uint32_t i=0; i<root_dir_sectors; i++) {
            uint32_t lba = OFFSET_FAT + first_root_dir_sector + i;
            ahci_read(sataport, lba, 0, 1, (uint16_t*)buf);
            fat_dir_entry_t *entries = (fat_dir_entry_t*)buf;
            for(uint32_t j=0; j<entries_per_sector; j++) {
                if(cb(&entries[j], lba, j, ctx)) { free(buf); return; }
            }
        }
    } else {
        uint32_t cluster = (start_cluster == 0) ? root_cluster_32 : start_cluster;
        uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
        
        while(cluster < eof && cluster != 0) {
            uint32_t lba_base = fat_cluster_to_lba(cluster);
            for(uint32_t i=0; i<fat->sectors_per_cluster; i++) {
                uint32_t lba = lba_base + i;
                ahci_read(sataport, lba, 0, 1, (uint16_t*)buf);
                fat_dir_entry_t *entries = (fat_dir_entry_t*)buf;
                for(uint32_t j=0; j<entries_per_sector; j++) {
                    if(cb(&entries[j], lba, j, ctx)) { free(buf); return; }
                }
            }
            cluster = fat_read_fat_entry(cluster);
        }
    }
    free(buf);
}

// Context structures for callbacks
typedef struct {
    const char *name;
    fat_dir_entry_t result;
    bool found;
    uint32_t sector_lba;
    uint32_t entry_offset;
} find_ctx_t;

bool find_entry_cb(fat_dir_entry_t *entry, uint32_t sector_lba, uint32_t entry_idx, void *p) {
    find_ctx_t *ctx = (find_ctx_t*)p;
    if(entry->file_name[0] == 0) return false; // End of dir, actually could stop here optimization?
    if(entry->file_name[0] == 0xE5) return false; // Deleted

    if((entry->attribute_file & 0x0F) != 0x0F) { // Skip LFN
        // Compare names (8.3 format in entry, 11 chars)
        // ctx->name should be 11 chars prepared
        if(memcmp(entry->file_name, ctx->name, 11) == 0) {
            ctx->result = *entry;
            ctx->found = true;
            ctx->sector_lba = sector_lba;
            ctx->entry_offset = entry_idx;
            return true; // Stop
        }
    }
    return false;
}

// Helper to find entry in a specific directory cluster
bool fat_find_entry_in_dir(uint32_t dir_cluster, const char *name_11, fat_dir_entry_t *out_entry) {
    find_ctx_t ctx;
    ctx.name = name_11;
    ctx.found = false;
    fat_foreach_entry(dir_cluster, find_entry_cb, &ctx);
    if(ctx.found && out_entry) *out_entry = ctx.result;
    return ctx.found;
}

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

// Recursive path finder
fat_dir_entry_t *get_entry_with_path(const char *path, uint32_t *_cluster) {
    char name_buf[12];
    int path_len = strlen(path);
    char *path_copy = (char*)malloc(path_len + 1, 4);
    strcpy(path_copy, path);
    
    char *token = strtok(path_copy, "/");
    uint32_t current_cluster = 0; // Root
    
    // Handle root "/" path
    if(!token) {
        free(path_copy);
        if(_cluster) *_cluster = (fat_type == FAT32 ? root_cluster_32 : 0);
        return NULL; // Return NULL but with root cluster set? Or handle differently.
    }
    
    fat_dir_entry_t *found_entry = (fat_dir_entry_t*)malloc(sizeof(fat_dir_entry_t), 4);
    bool found = false;
    
    while(token != NULL) {
        // Format to 11 chars
        memset(name_buf, ' ', 11);
        int len = strlen(token);
        if(len > 11) len = 11;
        // memcpy(name_buf, token, len);
        fat_translate(token, name_buf);
        
        find_ctx_t ctx;
        ctx.name = name_buf;
        ctx.found = false;
        
        fat_foreach_entry(current_cluster, find_entry_cb, &ctx);
        
        if(!ctx.found) {
            printf("Path not found: %s\n", token);
            free(path_copy); free(found_entry); return NULL;
        }
        
        *found_entry = ctx.result;
        current_cluster = (found_entry->first_cluster_high << 16) | found_entry->first_cluster_low;
        token = strtok(NULL, "/");
        found = true;
    }
    
    free(path_copy);
    if(_cluster) *_cluster = current_cluster;
    return found_entry;
}

// Listing (returns buffer of entries)
// For compatibility with main.c
// Implementation using iteration? Or just read-all as before for simplicity of return type
fat_dir_entry_t *get_entries_with_path(const char *path, uint32_t *_cluster, uint32_t *_total_clusters) {
    uint32_t cluster = 0;
    
    // Resolve path first
    // Checks if path exists.
    // If path is root "/", returns root entries.
    // get_entry_with_path returns NULL for root.
    
    fat_dir_entry_t *entry = get_entry_with_path(path, &cluster);
    
    // If entry found, and it's a directory, list it.
    // If entry not found but path was root, list root.
    
    uint32_t target_cluster = 0;
    
    if(entry) {
        if(entry->attribute_file & 0x10) {
            target_cluster = cluster;
        } else {
            // It's a file, returns NULL
            free(entry); return NULL;
        }
        free(entry); 
    } else {
        // Assume root if path is root-like
        if(path == NULL || strlen(path) == 0 || (path[0] == '/' && strlen(path) == 1)) {
            target_cluster = (fat_type == FAT32) ? root_cluster_32 : 0;
        } else {
            return NULL; 
        }
    }
    
    if(_cluster) *_cluster = target_cluster;
    
    // READ ALL entries in target_cluster
    // Calculate size
    uint32_t count = 0;
    uint32_t temp = target_cluster;
    if(fat_type == FAT16 && target_cluster == 0) {
        count = root_dir_sectors; // Sectors, not clusters
    } else {
        if(temp == 0) temp = root_cluster_32; // FAT32 Root
        uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
        while(temp < eof && temp != 0) {
            count++;
            temp = fat_read_fat_entry(temp);
        }
    }
    if(_total_clusters) *_total_clusters = count;
    
    // Allocate and Read
    // Note: FAT16 root is continuous sectors. FAT32/Subdirs are cluster chains.
    
    if(fat_type == FAT16 && target_cluster == 0) {
        uint32_t size = root_dir_sectors * FAT_SECTOR_SIZE;
        uint8_t *b = (uint8_t*)malloc(size, 4);
        ahci_read(sataport, OFFSET_FAT + first_root_dir_sector, 0, root_dir_sectors, (uint16_t*)b);
        return (fat_dir_entry_t*)b;
    } else {
        uint32_t buf_sz = count * fat->sectors_per_cluster * FAT_SECTOR_SIZE;
        uint8_t *b = (uint8_t*)malloc(buf_sz, 4);
        uint8_t *p = b;
        temp = (target_cluster == 0) ? root_cluster_32 : target_cluster;
        uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
        
        while(temp < eof && temp != 0) {
            ahci_read(sataport, fat_cluster_to_lba(temp), 0, fat->sectors_per_cluster, (uint16_t*)p);
            p += fat->sectors_per_cluster * FAT_SECTOR_SIZE;
            temp = fat_read_fat_entry(temp);
        }
        return (fat_dir_entry_t*)b;
    }
}

// Printing
bool print_cb(fat_dir_entry_t *e, uint32_t s, uint32_t o, void *p) {
    (void)s; (void)o; (void)p;
    if(e->file_name[0] == 0) return false;
    if(e->file_name[0] == 0xE5) return false;
    if((e->attribute_file & 0x0F) != 0x0F) {
        char name[12];
        memcpy(name, e->file_name, 11); name[11]=0;
        printf("File: %s Size: %d Clus: %d Type: %s\n", name, e->size_file, (e->first_cluster_high<<16)|e->first_cluster_low, e->attribute_file == 0x20 ? "File" : "Directory");
    }
    return false;
}
void listing_root_dir_print() {
    printf("Root Dir:\n");
    fat_foreach_entry(0, print_cb, NULL);
}


// --- Create / Edit ---

typedef struct {
    fat_dir_entry_t *new_entry;
    bool created;
} create_ctx_t;

bool create_cb(fat_dir_entry_t *entry, uint32_t sector_lba, uint32_t entry_idx, void *p) {
    create_ctx_t *ctx = (create_ctx_t*)p;
    if(entry->file_name[0] == 0 || entry->file_name[0] == 0xE5) {
        // Found empty slot
        // 1. Allocate cluster for CONTENT of file (if needed?)
        // The passed entry should ideally have cluster set if it needs one. 
        // If not, we allocate one.
        
        uint32_t content_cluster = (ctx->new_entry->first_cluster_high << 16) | ctx->new_entry->first_cluster_low;
        if(content_cluster == 0) {
            content_cluster = fat_alloc_cluster();
            if(content_cluster == 0) {
                printf("Disk Full\n"); return true; // Stop
            }
            ctx->new_entry->first_cluster_low = content_cluster & 0xFFFF;
            ctx->new_entry->first_cluster_high = (content_cluster >> 16) & 0xFFFF;
        }
        
        // 2. Write directory entry to disk
        // Read sector again to be safe? (cb gives us lba)
        uint8_t *buf = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
        ahci_read(sataport, sector_lba, 0, 1, (uint16_t*)buf);
        fat_dir_entry_t *entries = (fat_dir_entry_t*)buf;
        entries[entry_idx] = *ctx->new_entry;
        ahci_write(sataport, sector_lba, 0, 1, (uint16_t*)buf);
        free(buf);
        
        ctx->created = true;
        return true; // Stop
    }
    return false;
}

void create_entry(const char *path, fat_dir_entry_t *entry_input) {
    // path is the DIRECTORY where we want to create the entry.
    // e.g., "/" or "/EFI"
    
    printf("Creating in: '%s'\n", path);
    uint32_t dir_cluster = 0;
    
    // Resolve dir cluster
    if(path == NULL || strlen(path) == 0 || (path[0] == '/' && strlen(path) == 1)) {
        dir_cluster = 0; // Root
    } else {
        fat_dir_entry_t *d = get_entry_with_path(path, &dir_cluster);
        if(!d) {
             printf("Target dir not found: '%s'\n", path); return; 
        }
        free(d);
    }
    
    create_ctx_t ctx;
    ctx.new_entry = entry_input;
    ctx.created = false;
    
    fat_foreach_entry(dir_cluster, create_cb, &ctx);
    
    if(!ctx.created) {
        printf("Error: Could not create entry (directory full or disk error)\n");
    }
}


// --- Edit Helpers ---

typedef struct {
    const char *target_name;
    fat_dir_entry_t *new_entry;
} edit_ctx_t;

bool edit_cb(fat_dir_entry_t *e, uint32_t l, uint32_t i, void *p) {
    if(e->file_name[0] == 0 || e->file_name[0] == 0xE5) return false;
    edit_ctx_t *ctx = (edit_ctx_t*)p;
    if(memcmp(e->file_name, ctx->target_name, 11) == 0) {
        // Found it, write new data
        uint8_t *buf = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
        ahci_read(sataport, l, 0, 1, (uint16_t*)buf);
        fat_dir_entry_t *entries = (fat_dir_entry_t*)buf;
        
        entries[i] = *ctx->new_entry;
        
        ahci_write(sataport, l, 0, 1, (uint16_t*)buf);
        free(buf);
        return true; 
    }
    return false;
}

// Edit Entry: Find entry by checking name in directory, then update
void edit_entry(const char *path, fat_dir_entry_t *the_entry, fat_dir_entry_t *entry_input) {
     // Path is FULL path to file? Or directory?
     // Based on previous code flow: path passed is original path.
     // We need to find parent dir of 'path'.
     // But wait, user code passes 'path' to edit_entry.
     
     // Original logic: get_entries_with_path(parent_path).
     
     char parent[128]; strcpy(parent, path);
     char *last = strrchr(parent, '/');
     if(last) { *last = 0; if(strlen(parent)==0) strcpy(parent, "/"); } 
     else strcpy(parent, "/");
     
     uint32_t dir_cluster = 0;
     if(strcmp(parent, "/") != 0) {
         fat_dir_entry_t *d = get_entry_with_path(parent, &dir_cluster);
         if(!d) return; 
         free(d);
     }
     
     // Find and update
     edit_ctx_t ctx;
     ctx.target_name = the_entry->file_name;
     ctx.new_entry = entry_input;
     
     fat_foreach_entry(dir_cluster, edit_cb, &ctx);
}

void delete_entry(const char *path, fat_dir_entry_t *the_entry) {
    fat_dir_entry_t del = *the_entry;
    del.file_name[0] = 0xE5;
    edit_entry(path, the_entry, &del);
}


// --- Data IO ---

void write_data(const char *path, fat_dir_entry_t *the_entry, char *data, uint32_t size) {
    // 1. Resolve entry to get cluster
    // (Assuming entry exists)
    
    // This function takes entry struct? But path too?
    // It seems to update the size in the directory entry after writing.
    
    uint32_t cluster = (the_entry->first_cluster_high << 16) | the_entry->first_cluster_low;
    if(cluster == 0) {
        // Try creating? edit_entry usage suggests it exists.
        // If 0, maybe alloc first?
        cluster = fat_alloc_cluster();
        the_entry->first_cluster_low = cluster & 0xFFFF;
        the_entry->first_cluster_high = (cluster >> 16) & 0xFFFF;
        // Need to update directory entry with new cluster
        edit_entry(path, the_entry, the_entry);
    }
    
    uint32_t written = 0;
    uint32_t current = cluster;
    uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
    uint8_t *sec = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
    
    while(written < size) {
        uint32_t lba = fat_cluster_to_lba(current);
        for(int i=0; i<fat->sectors_per_cluster; i++) {
             memset(sec, 0, FAT_SECTOR_SIZE);
             uint32_t chunk = FAT_SECTOR_SIZE;
             if(written + chunk > size) chunk = size - written;
             memcpy(sec, data + written, chunk);
             ahci_write(sataport, lba + i, 0, 1, (uint16_t*)sec);
             written += chunk;
             if(written >= size) break;
        }
        
        if(written < size) {
            uint32_t next = fat_read_fat_entry(current);
            if(next >= eof || next == 0) {
                next = fat_alloc_cluster();
                fat_write_fat_entry(current, next);
            }
            current = next;
        }
    }
    free(sec);
    
    the_entry->size_file = size;
    edit_entry(path, the_entry, the_entry);
}

char *read_data(const char *path, fat_dir_entry_t *the_entry, uint32_t *size) {
    // Read content
    uint32_t cluster = (the_entry->first_cluster_high << 16) | the_entry->first_cluster_low;
    uint32_t fsize = the_entry->size_file;
    if(size) *size = fsize;
    
    if(fsize == 0) return NULL;
    
    char *buf = (char*)malloc(fsize + 1, 4);
    memset(buf, 0, fsize + 1);
    
    uint32_t read = 0;
    uint32_t current = cluster;
    uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
    uint8_t *sec = (uint8_t*)malloc(FAT_SECTOR_SIZE, 4);
    
    while(current < eof && current != 0 && read < fsize) {
        uint32_t lba = fat_cluster_to_lba(current);
        for(int i=0; i<fat->sectors_per_cluster; i++) {
            ahci_read(sataport, lba + i, 0, 1, (uint16_t*)sec);
            uint32_t chunk = FAT_SECTOR_SIZE;
            if(read + chunk > fsize) chunk = fsize - read;
            memcpy(buf + read, sec, chunk);
            read += chunk;
            if(read >= fsize) break;
        }
        current = fat_read_fat_entry(current);
    }
    free(sec);
    return buf;
}

// Wrappers for direct path IO (used by adapter)
void write_data_direct_path(const char *path, char *data, uint32_t size) {
    uint32_t c;
    fat_dir_entry_t *e = get_entry_with_path(path, &c);
    if(e) {
        write_data(path, e, data, size);
        free(e);
    }
}
char *read_data_direct_path(const char *path, uint32_t *size) {
    uint32_t c;
    fat_dir_entry_t *e = get_entry_with_path(path, &c);
    if(e) {
        char *d = read_data(path, e, size);
        free(e);
        return d;
    }
    return NULL;
}

// --- Compatibility Wrappers ---

fat_dir_entry_t *listing_root_dir(uint32_t *total_clusters, uint32_t *total_sectors) {
    if(fat_type == FAT16) {
        // FAT16 root is not clusters, it's sectors.
        // But get_entries_with_path handles "/" specially.
        // However, get_entries_with_path returns malloc'd buffer of ALL entries.
        // It uses sectors for FAT16 root internally.
        
        // We can just call get_entries_with_path("/")
        // But get_entries_with_path returns clusters count in 2nd arg.
        // For FAT16 root, it returns sector count in 2nd arg (see implementation).
        
        uint32_t count=0;
        fat_dir_entry_t *res = get_entries_with_path("/", &count, NULL);
        if(total_sectors) *total_sectors = count; 
        // total_clusters arg is ignored for FAT16 root usually or set to 0?
        if(total_clusters) *total_clusters = 0; 
        return res;
    } else {
        uint32_t count=0;
        fat_dir_entry_t *res = get_entries_with_path("/", NULL, &count);
        if(total_clusters) *total_clusters = count;
        return res;
    }
}

fat_dir_entry_t *listing_dir(uint32_t *total_clusters, uint32_t cluster) {
    // This function takes a cluster and returns entries.
    // get_entries_with_path takes a path.
    // We don't have a path for an arbitrary cluster easily.
    // We must implement this manually using our internal reader.
    
    // Use same logic as get_entries_with_path but starting at cluster.
    
    uint32_t count = 0;
    uint32_t temp = cluster;
    uint32_t eof = (fat_type == FAT32) ? FAT32_EOF : FAT16_EOF;
    
    // Count size
    while(temp < eof && temp != 0) {
        count++;
        temp = fat_read_fat_entry(temp);
    }
    if(total_clusters) *total_clusters = count;
    
    uint32_t buf_sz = count * fat->sectors_per_cluster * FAT_SECTOR_SIZE;
    uint8_t *b = (uint8_t*)malloc(buf_sz, 4);
    uint8_t *p = b;
    temp = cluster;
    
    while(temp < eof && temp != 0) {
        ahci_read(sataport, fat_cluster_to_lba(temp), 0, fat->sectors_per_cluster, (uint16_t*)p);
        p += fat->sectors_per_cluster * FAT_SECTOR_SIZE;
        temp = fat_read_fat_entry(temp);
    }
    return (fat_dir_entry_t*)b;
}

void listing_dir_print(uint32_t active_cluster) {
    printf("Listing Cluster %d\n", active_cluster);
    fat_foreach_entry(active_cluster, print_cb, NULL);
}
