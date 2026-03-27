#include "paging.h"
#include <limine.h>

// Request Memory Map
extern volatile struct limine_memmap_request memmap_request;

// Request HHDM (Important for accessing page tables)
__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

typedef uint64_t pt_entry;


// Helper to get virtual address from physical address using HHDM
#define PHYS_TO_VIRT(addr) ((void*)((uint64_t)(addr) + hhdm_request.response->offset))


static inline uint64_t read_cr3(void) {
    uint64_t value;
    __asm__("mov %%cr3, %0" : "=r" (value));
    return value;
}

uint64_t *get_pml4() {
	uint64_t cr3 = read_cr3() & ~0xFFFULL;
	return (uint64_t *)PHYS_TO_VIRT(cr3);
}

bool check_present(uint64_t *pml4, uint64_t virt, uint64_t phys) {
	size_t pml4_idx = (virt >> 39) & 0x1FF;
	size_t pdpt_idx = (virt >> 30) & 0x1FF;
	size_t pd_idx = (virt >> 21) & 0x1FF;
	size_t pt_idx = (virt >> 12) & 0x1FF;

	uint64_t *pdpt = get_next_level(pml4, pml4_idx);
	uint64_t *pd = get_next_level(pdpt, pdpt_idx);
	uint64_t *pt = get_next_level(pd, pd_idx);
	if(!(pt[pt_idx] & PTE_PRESENT)) return false;

	return true;
}

void map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
	size_t pml4_idx = (virt >> 39) & 0x1FF;
	size_t pdpt_idx = (virt >> 30) & 0x1FF;
	size_t pd_idx = (virt >> 21) & 0x1FF;
	size_t pt_idx = (virt >> 12) & 0x1FF;

	uint64_t *pdpt = get_next_level(pml4, pml4_idx);
	uint64_t *pd = get_next_level(pdpt, pdpt_idx);
	uint64_t *pt = get_next_level(pd, pd_idx);

	pt[pt_idx] = phys | flags | PTE_PRESENT;
    	asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t *get_next_level(uint64_t *current_level, size_t entry_idx) {
    if (current_level[entry_idx] & PTE_HUGE) {
        print_str("FATAL: tried to walk into huge page\n");
	uint64_t val = current_level[entry_idx];
	print_str("Entry value: ");
	print_hex(val);
	write_terminal('\n');

        while (1) asm volatile("hlt");
    }

    if (!(current_level[entry_idx] & PTE_PRESENT)) {
        uint64_t new_table_phys = allocate_frame();
        if (!new_table_phys) {
            print_str("FATAL: OOM in get_next_level");
            while (1) asm volatile("hlt");
        }
	if (new_table_phys & 0xFFF) {
	    print_str("FATAL: unaligned frame\n");
    		while (1) asm volatile("hlt");
	}


        current_level[entry_idx] =
            new_table_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;

        uint64_t *new_table = PHYS_TO_VIRT(new_table_phys);
        for (int i = 0; i < 512; i++) new_table[i] = 0;
    }

    // Ensure PTE_USER is set on the path to a user page
    current_level[entry_idx] |= (PTE_USER); 

    return PHYS_TO_VIRT(current_level[entry_idx] & PTE_ADDR_MASK);
}


/*
void map_page_huge(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
	size_t pml4_idx = (virt >> 39) & 0x1FF;
	size_t pdpt_idx = (virt >> 30) & 0x1FF;
	size_t pd_idx = (virt >> 21) & 0x1FF;

	uint64_t *pdpt = get_next_level(pml4, pml4_idx);
	uint64_t *pd = get_next_level(pdpt, pdpt_idx);

	pd[pd_idx] = phys | flags | PTE_PRESENT | PTE_HUGE;
}
*/

uint64_t *init_paging() {
	if(memmap_request.response == NULL) {
		print_str("paging memmap tidak ada response");
		while(1) { asm volatile("hlt");}
	}
	if(hhdm_request.response == NULL) {
		print_str("paging hhdm tidak ada response");
		while(1) { asm volatile("hlt");}
	}


	uint64_t phys = allocate_frame();
	uint64_t *pml4 = PHYS_TO_VIRT(phys);

	uint64_t page_size = 4096;
	
	// Map kernel (higher half)
	extern uint64_t KERNEL_PHYS_START, KERNEL_PHYS_END;
	extern uint64_t KERNEL_VIRT_START, KERNEL_VIRT_END;
	print_str("kernel phys start: ");
	print_hex(KERNEL_PHYS_START);
	print_str("kernel phys end: ");
	print_hex(KERNEL_PHYS_END);
	print_str("kernel virt start: ");
	print_hex(KERNEL_VIRT_START);
	print_str("kernel virt end: ");
	print_hex(KERNEL_VIRT_END);
	uint64_t ksize = KERNEL_PHYS_END - KERNEL_PHYS_START;
	for(uint64_t off = 0; off < ksize; off += page_size) {
		map_page(pml4, KERNEL_VIRT_START + off, KERNEL_PHYS_START + off, PTE_WRITABLE);
	}
	// Map first 2MB identity (for compatibility)
	for(uint64_t a = 0; a < 0x00200000ULL; a += page_size) {
		map_page(pml4, a, a, PTE_WRITABLE);
	}
	
	// Map HHDM (entire physical memory accessible at offset)
	// Map first 4GB of physical memory via HHDM using 2MB HUGE PAGES
	uint64_t hhdm_offset = hhdm_request.response->offset;
	uint64_t huge_page_size = 0x200000;
	for(uint64_t phys = 0; phys < 0x100000000ULL; phys += huge_page_size) {
		map_page_huge(pml4, hhdm_offset + phys, phys, PTE_WRITABLE);
	}
	
	return pml4;
}

void enable_paging(uint64_t *pml4) {
	uint64_t pml4_phys = (uint64_t)pml4 - hhdm_request.response->offset;
	asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

void load_cr3(uint64_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}