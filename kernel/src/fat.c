#include "fat.h"

#define OFFSET_FAT 2048

enum FatType fat_type;

static uint8_t fat_bpb_buffer[512];
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

uint32_t first_root_dir_sector = 0;

// FAT 32 
uint32_t root_cluster_32 = 0;
uint32_t first_sector_of_cluster = 0;

// this for read ahci because we dont want to use all memory just for read and write right?
static uint64_t buf_phys;

void fat_init() {
	buf_phys = allocate_frame();	
	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	memset(buf, 0, 512);
	bool success = ahci_read(sataport, OFFSET_FAT, 0, 1, buf);
	if(success) {
		memcpy(fat, buf, 512);
		if(fat->table_size_16 == 0) {
			fat_32 = (fat_extBS_32_t *)fat->extended_section;
			fat_type = FAT32;
			total_sectors = fat->total_sectors_32;
			fat_size = fat_32->table_size_32;
			first_fat_sector = fat->reserved_sector_count;
			first_data_sector = fat->reserved_sector_count + (fat->table_count * fat_size);
			root_cluster_32 = fat_32->root_cluster;
			first_sector_of_cluster = ((root_cluster_32 - 2) * fat->sectors_per_cluster) + first_data_sector;
			printf("total sectors %d\n", total_sectors);
			printf("fat size %d\n", fat_size);
		}
		else {
			fat_16 = (fat_extBS_16_t *)fat->extended_section;
			fat_type = FAT16;
			total_sectors = fat->total_sectors_16;
			fat_size = fat->table_size_16;
			root_dir_sectors = ((fat->root_entry_count * 32) + (fat->bytes_per_sector - 1)) / fat->bytes_per_sector;
			first_data_sector = fat->reserved_sector_count + (fat->table_count * fat_size) + root_dir_sectors;
			first_fat_sector = fat->reserved_sector_count;
			data_sector = total_sectors - (fat->reserved_sector_count + (fat->table_count * fat_size) + root_dir_sectors);
			total_clusters = data_sector / fat->sectors_per_cluster;
			first_root_dir_sector = first_data_sector - root_dir_sectors;
			printf("total sectors %d\n", total_sectors);
			printf("fat size %d\n", fat_size);
		}
	}
	else {
		printf("AHCI can't read");
	}
}

uint16_t FAT16_read(uint16_t active_cluster) {
	uint16_t sector_size = fat->bytes_per_sector;
	uint8_t FAT_table[sector_size];
	uint32_t fat_offset = active_cluster * 2;
	uint32_t fat_sector = first_fat_sector + (fat_offset / sector_size);
	uint32_t ent_offset = fat_offset % sector_size;

	if (fat_sector >= fat_size) {
    		printf("Error: Try reading outside the FAT table!\n");
    		return 0xFFFF; // Cancel reading
	}


	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, buf);

	memcpy(FAT_table, buf, sector_size);

	uint16_t table_value = *(uint16_t *)&FAT_table[ent_offset];

	return table_value;
}

uint32_t FAT32_read(uint32_t active_cluster) {
	uint16_t sector_size = fat->bytes_per_sector;
	uint8_t FAT_table[sector_size];
	uint32_t fat_offset = active_cluster * 4;
	uint32_t fat_sector = first_fat_sector + (fat_offset / sector_size);
	uint32_t ent_offset = fat_offset % sector_size;
	// Use variables already taken from boot sector
	if (fat_sector >= fat_size) {
    		printf("Error: Try reading outside the FAT table!\n");
    		return 0xFFFFFFFF; // Cancel reading
	}


	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	memset(buf, 0, 512);
	ahci_read(sataport, OFFSET_FAT + fat_sector, 0, 1, buf);

	memcpy(FAT_table, buf, sector_size);

	uint32_t table_value = *(uint32_t *)&FAT_table[ent_offset];

	table_value &= 0x0FFFFFFF;

	return table_value;
}

uint32_t cluster_to_LBA(uint32_t cluster) {
	return ((cluster - 2) * fat->sectors_per_cluster) + first_data_sector;
}

#define ENTRY_SIZE 32

void listing_root_dir_print() {
	const uint32_t ENTRIES_SIZE = fat->bytes_per_sector / ENTRY_SIZE;
	printf("now listing root directory\n");
	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	if(fat_type == FAT32) {
		uint32_t table_value = root_cluster_32;
		do {
			memset(buf, 0, 512);
			ahci_read(sataport, OFFSET_FAT + cluster_to_LBA(table_value), 0, 1, buf);
			fat_dir_entry_t *entries = (fat_dir_entry_t *)buf;
			for(uint32_t i = 0; i < ENTRIES_SIZE; i++) {
				fat_dir_entry_t *entry = (fat_dir_entry_t *)&entries[i];
				if(entry->file_name[0] == 0) continue;
				if(entry->file_name[0] == 0xE5) continue;
				printf("name file %s\n", entry->file_name);
				printf("first cluster high %d\n", entry->first_cluster_high);
				printf("first cluster low  %d\n", entry->first_cluster_low);
			}
			table_value = FAT32_read(table_value);
		}while(table_value < 0x0FFFFFF8);
	}
	else if(fat_type == FAT16) {
		for(uint32_t sector = 0; sector < root_dir_sectors; sector++) {
			memset(buf, 0, 512);
			ahci_read(sataport, OFFSET_FAT + first_root_dir_sector + sector, 0, 1, buf);
			fat_dir_entry_t *entries = (fat_dir_entry_t *)buf;
			for(int i = 0; i < ENTRIES_SIZE; i++) {
				fat_dir_entry_t *entry = (fat_dir_entry_t *)&entries[i];
				if(entry->file_name[0] == 0) continue;
				if(entry->file_name[0] == 0xE5) continue;
				printf("nama file %s\n", entry->file_name);
				printf("first cluster high %d\n", entry->first_cluster_high);
				printf("first cluster low %d\n", entry->first_cluster_low);
			}
		}
	}
}

void listing_dir_print(uint32_t active_cluster) {
	printf("now listing directory\n");
	printf("start cluster %d\n", active_cluster);
	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	const uint32_t ENTRIES_SIZE = fat->bytes_per_sector / ENTRY_SIZE;
	if(fat_type == FAT32) {
		uint32_t table_value = active_cluster;
		do {
			memset(buf, 0, 512);
			ahci_read(sataport, OFFSET_FAT + cluster_to_LBA(table_value), 0, 1, buf);
			fat_dir_entry_t *entries = (fat_dir_entry_t *)buf;
			for(uint32_t i = 0; i < ENTRIES_SIZE; i++) {
				fat_dir_entry_t *entry = (fat_dir_entry_t *)&entries[i];
				if(entry->file_name[0] == 0) continue;
				if(entry->file_name[0] == 0xE5) continue;
				printf("nama file %s\n", entry->file_name);
				printf("first cluster high %d\n", entry->first_cluster_high);
				printf("first cluster low %d\n", entry->first_cluster_low);
			}
			table_value = FAT32_read(table_value);
		}while(table_value < 0x0FFFFFF8);
	}
	else if(fat_type == FAT16) {
		uint32_t table_value = active_cluster;
		do {
			memset(buf, 0, 512);
			bool success = ahci_read(sataport, OFFSET_FAT + cluster_to_LBA(table_value), 0, 1, buf);
			if(success == false) {
				printf("failed to read disk");
				return;
			}
			fat_dir_entry_t *entries = (fat_dir_entry_t *)buf;
			for(int i = 0; i < ENTRIES_SIZE; i++) {
				fat_dir_entry_t *entry = (fat_dir_entry_t *)&entries[i];
				if(entry->file_name[0] == 0) continue;
				if(entry->file_name[0] == 0xE5) continue;
				printf("nama file %s\n", entry->file_name);
				printf("first cluster high %d\n", entry->first_cluster_high);
				printf("first cluster low %d\n", entry->first_cluster_low);
				bool isDir = entry->attribute_file & 0x10;
				printf("is dir %d\n", isDir);
			}
			table_value = FAT16_read(table_value);
		} while(table_value < 0xFFF8);
	}
}
