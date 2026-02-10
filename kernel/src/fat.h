#pragma once
#include "main.h"
#include "ahci.h"
#include "memory.h"
#include "paging.h"
#include "util.h"

enum FatType {
	FAT16,
	FAT32
};

typedef struct fat_extBS_32
{
	//extended fat32 stuff
	unsigned int		table_size_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];

}__attribute__((packed)) fat_extBS_32_t;

typedef struct fat_extBS_16
{
	//extended fat12 and fat16 stuff
	unsigned char		bios_drive_num;
	unsigned char		reserved1;
	unsigned char		boot_signature;
	unsigned int		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
	
}__attribute__((packed)) fat_extBS_16_t;

typedef struct fat_BS
{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short		bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		table_count;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16;
	unsigned char		media_type;
	unsigned short		table_size_16;
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32;
	
	//this will be cast to it's specific type once the driver actually knows what type of FAT this is.
	unsigned char		extended_section[54];
	
}__attribute__((packed)) fat_BS_t;

/*
typedef struct fat_dir_entry {
	char file_name[11];
	uint8_t attribute_file;
	uint8_t reserved;
	uint8_t creation_time;
	uint16_t time_created;
	uint16_t date_created;
	uint16_t last_accessed_date;
	uint16_t first_cluster_high;
	uint16_t last_modified_time;
	uint16_t last_modified_date;
	uint16_t first_cluster_low;
	uint32_t size_file;
} __attribute__((packed)) fat_dir_entry_t;
*/

// long file name
typedef struct fat_dir_entry_long {
	uint8_t order;
	uint16_t first_5_chars[5];
	uint8_t attribute;
	uint8_t type;
	uint8_t checksum;
	uint16_t next_6_chars[3];
	uint16_t always0; // always 0
	uint16_t last_2_chars[2];
} __attribute__((packed)) fat_dir_entry_long_t;

typedef struct fat_dir_entry {
    char file_name[11];             // 8 bytes name, 3 bytes extension
    uint8_t attribute_file;         // File attributes (ReadOnly, Hidden, Directory, etc.)
    uint8_t reserved;               // Reserved for Windows NT
    uint8_t creation_time_tenth;    // Creation time in hundredths of a second
    uint16_t time_created;          // Creation time (2-second granularity)
    uint16_t date_created;          // Creation date
    uint16_t last_accessed_date;    // Last access date
    uint16_t first_cluster_high;    // High 16-bit of cluster number (FAT32 only)
    uint16_t last_modified_time;    // Last modification time
    uint16_t last_modified_date;    // Last modification date
    uint16_t first_cluster_low;     // Low 16-bit of cluster number
    uint32_t size_file;             // File size in bytes
} __attribute__((packed)) fat_dir_entry_t;

typedef struct MBR {
	uint8_t boot_code[440];
	uint8_t disk_signature[4];
	uint16_t reserved_1;
	uint8_t partition_table[64];
	uint16_t signature;
} __attribute__((packed)) MBR_t;

typedef struct partition_entry {
	uint8_t bootable;
	uint8_t CHS_start_head;
	uint8_t CHS_start_sector_and_cylinder_low;
	uint8_t CHS_start_cylinder_high;
	uint8_t type;
	uint8_t CHS_end_head;
	uint8_t CHS_end_sector_and_cylinder_low;
	uint8_t CHS_end_cylinder_high;
	uint32_t LBA_start_sector;
	uint32_t total_sectors;
} __attribute__((packed)) partition_entry_t;

void fat_init();
void listing_root_dir_print();
void listing_dir_print(uint32_t active_cluster);
void FAT16_write(uint16_t active_cluster, uint16_t cluster);
uint16_t FAT16_read(uint16_t active_cluster);
uint32_t FAT32_read(uint32_t active_cluster);
void FAT32_write(uint32_t active_cluster, uint32_t cluster);
fat_dir_entry_t *listing_root_dir(uint32_t *total_clusters, uint32_t *total_sectors);
fat_dir_entry_t *listing_dir(uint32_t *total_clusters, uint32_t cluster);
fat_dir_entry_t *get_entries_with_path(const char *path, uint32_t *_cluster, uint32_t *_total_clusters);
fat_dir_entry_t *get_entry_with_path(const char *path, uint32_t *_cluster);
void create_entry(const char *path, fat_dir_entry_t *entry_input);
void write_data(const char *path, fat_dir_entry_t *the_entry, char *data, uint32_t size);
char *read_data(const char *path, fat_dir_entry_t *the_entry, uint32_t *size);
char *read_data_direct_path(const char *path, uint32_t *size);
void write_data_direct_path(const char *path, char *data, uint32_t size);