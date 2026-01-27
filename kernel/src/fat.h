#pragma once
#include "main.h"
#include "ahci.h"
#include "memory.h"
#include "paging.h"

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

typedef struct fat_dir_entry {
    char file_name[11];             // 8 bytes nama, 3 bytes ekstensi
    uint8_t attribute_file;         // Atribut file (ReadOnly, Hidden, Directory, dll)
    uint8_t reserved;               // Reserved untuk Windows NT
    uint8_t creation_time_tenth;    // Waktu pembuatan dalam perseratus detik
    uint16_t time_created;          // Waktu pembuatan (Granularitas 2 detik)
    uint16_t date_created;          // Tanggal pembuatan
    uint16_t last_accessed_date;    // Tanggal terakhir diakses
    uint16_t first_cluster_high;    // 16-bit atas dari nomor cluster (Hanya FAT32)
    uint16_t last_modified_time;    // Waktu terakhir dimodifikasi
    uint16_t last_modified_date;    // Tanggal terakhir dimodifikasi
    uint16_t first_cluster_low;     // 16-bit bawah dari nomor cluster
    uint32_t size_file;             // Ukuran file dalam bytes
} __attribute__((packed)) fat_dir_entry_t;

void fat_init();
void listing_root_dir_print();
void listing_dir_print(uint32_t active_cluster);
