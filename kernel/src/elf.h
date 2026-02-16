#pragma once
#include <stdint.h>
#include "vfs.h"
#include "terminal.h"
#include "paging.h"

typedef struct ELF_HEADER {
    uint8_t magic[4];
    uint8_t class;
    uint8_t data;
    uint8_t version;
    uint8_t os_abi;
    uint8_t abi_version;
    uint8_t padding[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry_point;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_entry_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_entry_count;
    uint16_t section_name_string_table_index;
} ELF_HEADER_t;

typedef struct ELF_PROGRAM_HEADER {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
} ELF_PROGRAM_HEADER_t;

typedef struct ELF_SECTION_HEADER {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t address_align;
    uint64_t entry_size;
} ELF_SECTION_HEADER_t;

void load_elf(const char *path);