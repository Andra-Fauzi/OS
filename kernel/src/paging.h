#pragma once
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "terminal.h"
uint64_t *get_next_level(uint64_t *current_level, size_t entry_idx);
void map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t *init_paging();
void enable_paging(uint64_t *pml4);
uint64_t *get_pml4();
void map_page_huge(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#include <limine.h>
extern volatile struct limine_hhdm_request hhdm_request;
#define PHYS_TO_VIRT(addr) ((void*)((uint64_t)(addr) + hhdm_request.response->offset))
#define VIRT_TO_PHYS(addr) ((uint64_t)((uint64_t)(addr) - hhdm_request.response->offset))

#define PTE_PRESENT (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY (1ULL << 6)
#define PTE_HUGE (1ULL << 7)
#define PTE_GLOBAL (1ULL << 8)
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

#define PAGE_SIZE 4096ULL
