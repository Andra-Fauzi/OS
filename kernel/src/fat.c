#include "fat.h"

enum FatType fat_type;

void fat_init() {
	uint64_t buf_phys = allocate_frame();	
	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	bool success = ahci_read(sataport, 2048, 0, 1, buf);
	if(success) {
		fat_BS_t *FAT = (fat_BS_t *)buf;
		if(FAT->table_size_16 == 0) {
			fat_extBS_32_t *FAT32 = (fat_extBS_32_t *)FAT->extended_section;
			printf("fat version: %x", FAT32->fat_version);
			printf("total sector: %d", FAT->total_sectors_32);
		}
	}
	else {
		printf("AHCI tidak bisa baca");
	}
}
