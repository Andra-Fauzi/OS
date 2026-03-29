#include "elf.h"

typedef void (*entry_t)(void);

extern void jump_to_usermode(uint64_t entry_point, uint64_t user_stack);

ELF_HEADER_t *load_elf(const char *path, uint64_t *pml4) {
    asm volatile("cli");
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open ELF file\n");
        return NULL;
    }
    // MUST heap-allocate: returning a pointer to a stack local is UB —
    // the caller (run_elf) would read garbage entry_point and page-fault.
    ELF_HEADER_t *elf_header = (ELF_HEADER_t *)malloc(sizeof(ELF_HEADER_t), 8);
    if (!elf_header) {
        printf("Failed to allocate ELF header\n");
        return NULL;
    }
    memset(elf_header, 0, sizeof(ELF_HEADER_t));
    int result = vfs_read(fd, (char*)elf_header, sizeof(ELF_HEADER_t));
    if(result == -1) {
        printf("Failed to read ELF file\n");
        free(elf_header);
        return NULL;
    }

    char magic_str[4];
    magic_str[0] = 0x7F;
    magic_str[1] = 'E';
    magic_str[2] = 'L';
    magic_str[3] = 'F';

    if(memcmp(elf_header->magic, magic_str, 4) != 0) {
        printf("Invalid ELF file\n");
        free(elf_header);
        return NULL;
    }

    printf("ELF MAGIC : %s\n", elf_header->magic);
    printf("elf entry_point is %x\n", elf_header->entry_point);

    result = vfs_seek(fd, elf_header->program_header_offset);
    if(result == -1) {
        printf("error vfs seek\n");
        free(elf_header);
        return NULL;
    }
    
    for(size_t i = 0; i < elf_header->program_header_entry_count; i++) {
        ELF_PROGRAM_HEADER_t program_header;
        result = vfs_seek(fd, elf_header->program_header_offset + (sizeof(ELF_PROGRAM_HEADER_t) * i));
        if(result == -1) {
            printf("error vfs seek\n");
            free(elf_header);
            return NULL;
        }
        vfs_read(fd, (char*)&program_header, sizeof(ELF_PROGRAM_HEADER_t));
        
        printf("HEADER NO %d: type %d, flags %d, vaddr %x, filesz %d, memsz %d\n", 
               i, program_header.type, program_header.flags, (uint64_t)program_header.virtual_address, 
               (uint64_t)program_header.file_size, (uint64_t)program_header.memory_size);

        if(program_header.type == 1) { // PT_LOAD
            uint64_t vaddr = program_header.virtual_address;
            uint64_t memsize = program_header.memory_size;
            uint64_t filesize = program_header.file_size;
            uint64_t offset = program_header.offset;

            uint64_t first_page = vaddr & ~0xFFFULL;
            uint64_t last_page = (vaddr + memsize + 4095) & ~0xFFFULL;

            for (uint64_t page = first_page; page < last_page; page += 4096) {
                uint64_t phys = allocate_frame();
                map_page(pml4, page, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
                memset(PHYS_TO_VIRT(phys), 0, 4096);

                // Determining intersection with file data
                uint64_t page_vstart = page;
                uint64_t page_vend = page + 4096;
                uint64_t data_vstart = vaddr;
                uint64_t data_vend = vaddr + filesize;

                uint64_t intersect_vstart = (page_vstart > data_vstart) ? page_vstart : data_vstart;
                uint64_t intersect_vend = (page_vend < data_vend) ? page_vend : data_vend;

                if (intersect_vstart < intersect_vend) {
                    uint64_t copy_size = intersect_vend - intersect_vstart;
                    uint64_t file_read_off = offset + (intersect_vstart - vaddr);
                    uint64_t frame_off = intersect_vstart - page_vstart;
                    
                    vfs_seek(fd, file_read_off);
                    vfs_read(fd, (char*)PHYS_TO_VIRT(phys) + frame_off, copy_size);
                }
            }
        }
    }


    // entry_t entry = (entry_t)elf_header->entry_point;
    // entry();

    
    // Map entry point is handled by segments usually, but ensure it's mapped if needed
    // map_page(pml4, elf_header->entry_point & ~0xFFFULL, elf_header->entry_point & ~0xFFFULL, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    asm volatile("sti");
    return elf_header;
}

void run_elf(ELF_HEADER_t *elf_header) {
    uint64_t *pml4 = get_pml4();
    printf("start program at 0x%x\n", elf_header->entry_point);
    
    // Allocate and map user stack
    uint64_t stack_virt = USER_STACK_BASE;
    for(uint64_t offset = 0; offset < USER_STACK_SIZE; offset += PAGE_SIZE) {
        uint64_t phys = allocate_frame();
        if (phys == 0) {
            printf("Failed to allocate frame for user stack!\n");
            return;
        }
        map_page(pml4, stack_virt + offset, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
    }
    
    printf("User stack mapped from 0x%x to 0x%x\n", stack_virt, USER_STACK_TOP);
    
    // Jump to usermode with RSP pointing to the TOP of the stack
    printf("starting\n");
    jump_to_usermode(elf_header->entry_point, (USER_STACK_TOP - 16) & ~0xF);
    printf("end program\n");
}
