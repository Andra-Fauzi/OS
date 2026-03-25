#include "elf.h"

typedef void (*entry_t)(void);

extern void jump_to_usermode(uint64_t entry_point, uint64_t user_stack);

ELF_HEADER_t *load_elf(const char *path) {
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
        return NULL;
    }

    printf("ELF MAGIC : %s\n", elf_header->magic);
    printf("elf class is %d\n", (uint64_t)elf_header->class);
    printf("elf version is %d\n", (uint64_t)elf_header->version);
    printf("elf type is %d\n", (uint64_t)elf_header->type);
    printf("elf machine is %x\n", elf_header->machine);
    printf("elf program_header_entry_count is %d\n", (uint64_t)elf_header->program_header_entry_count);
    printf("elf section_header_entry_count is %d\n", (uint64_t)elf_header->section_header_entry_count);
    printf("elf program_header_offset is %d\n", (uint64_t)elf_header->program_header_offset);
    printf("elf entry_point is %x\n", elf_header->entry_point);

    
    uint64_t *pml4 = get_pml4();

    result = vfs_seek(fd, elf_header->program_header_offset);
    if(result == -1) {
        printf("error vfs seek\n");
        return NULL;
    }
    
    for(size_t i = 0; i < elf_header->program_header_entry_count; i++) {
        ELF_PROGRAM_HEADER_t program_header;
        result = vfs_seek(fd, elf_header->program_header_offset + (sizeof(ELF_PROGRAM_HEADER_t) * i));
        if(result == -1) {
            printf("error vfs seek\n");
            return NULL;
        }
        vfs_read(fd, (char*)&program_header, sizeof(ELF_PROGRAM_HEADER_t));
        
        printf("HEADER NO %d: type %d, flags %d, vaddr %x, filesz %d, memsz %d\n", 
               i, program_header.type, program_header.flags, (uint64_t)program_header.virtual_address, 
               (uint64_t)program_header.file_size, (uint64_t)program_header.memory_size);

        if(program_header.type == 1) { // PT_LOAD
            // Map all pages required for this segment
            uint64_t start = program_header.virtual_address;
            uint64_t end = start + program_header.memory_size;
            for (uint64_t addr = (start & ~0xFFFULL); addr < end; addr += 4096) {
                // Identity map for now as per current OS design
                map_page(pml4, addr, addr, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
            }

            // Use malloc instead of VLA to avoid stack overflow
            char *buf_data = (char*)malloc(program_header.memory_size, 1);
            if (!buf_data) {
                printf("Failed to allocate buffer for ELF segment\n");
                return NULL;
            }
            memset(buf_data, 0, program_header.memory_size);
            
            result = vfs_seek(fd, program_header.offset);
            if(result == -1) {
               printf("error vfs seek\n");
               free(buf_data);
               return NULL;
            }
            
            result = vfs_read(fd, buf_data, program_header.file_size);
            if(result == -1) {
               printf("error vfs read\n");
               free(buf_data);
               return NULL;
            }
            
            memcpy((void*)program_header.virtual_address, buf_data, program_header.memory_size);
            free(buf_data);
        }
    }

    // entry_t entry = (entry_t)elf_header->entry_point;
    // entry();

    
    // Map entry point if not already mapped by segments
    map_page(pml4, elf_header->entry_point & ~0xFFFULL, elf_header->entry_point & ~0xFFFULL, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    asm volatile("sti");
    return elf_header;
}

void run_elf(ELF_HEADER_t *elf_header) {
    uint64_t *pml4 = get_pml4();
    printf("start program at %x\n", elf_header->entry_point);
    
    // Allocate and map user stack
    uint64_t stack_phys = allocate_frame();
    uint64_t stack_virt = 0x70000000000; // Choose a high virtual address for user stack
    map_page(pml4, stack_virt, stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    
    printf("User stack at %x (phys %x)\n", stack_virt, stack_phys);
    // Jump to usermode with RSP pointing to the TOP of the stack
    printf("starting\n");
    jump_to_usermode(elf_header->entry_point, stack_virt + 4096);
    printf("end program\n");
}
