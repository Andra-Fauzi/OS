#include "memory.h"
#include "terminal.h"
#include <limine.h>

extern volatile struct limine_hhdm_request hhdm_request;

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0,
	.response = NULL
};

static volatile struct limine_memmap_response *memmap_response;

#define MAX_USABLE_REGIONS 64
static struct limine_memmap_entry *usable_regions[MAX_USABLE_REGIONS];
static size_t usable_region_count = 0;
static uint64_t next_frame_addr = 0;
static size_t current_region = 0;

#define PHYS_TO_VIRT(addr) ((void*)((uint64_t)(addr) + hhdm_request.response->offset))

#define FRAME_SIZE 4096

static uint16_t *frame_refs = NULL;
static uint64_t max_phys_global = 0;

static volatile int malloc_lock = 0;

static void lock_malloc() {
    while (__sync_lock_test_and_set(&malloc_lock, 1)) {
        asm volatile("pause");
    }
}

static void unlock_malloc() {
    __sync_lock_release(&malloc_lock);
}

void frame_allocator_init(void) {
	if(memmap_request.response == NULL) {
		print_str("frame allocator: memmap tidak ada response\n");
		while(1) {__asm__("hlt");}
	}
	memmap_response = memmap_request.response;
	for (size_t i = 0; i < memmap_response->entry_count; i++) {
		struct limine_memmap_entry *e = memmap_response->entries[i];
		if(e->type == LIMINE_MEMMAP_USABLE) {
			if(usable_region_count < MAX_USABLE_REGIONS) {
				usable_regions[usable_region_count++] = e;
			}
		}
        if (e->base + e->length > max_phys_global) {
            max_phys_global = e->base + e->length;
        }
	}
	if(usable_region_count > 0) {
		next_frame_addr = usable_regions[0]->base;
		if(next_frame_addr < 0x100000) {
			next_frame_addr = 0x100000;
		}
	}

    if (max_phys_global > 0) {
        uint64_t num_frames = max_phys_global / 4096;
        uint64_t refs_size = num_frames * sizeof(uint16_t);
        uint64_t first_frame = allocate_frame();
        if (first_frame != 0) {
            frame_refs = (uint16_t*)PHYS_TO_VIRT(first_frame);
            for(uint64_t size = 4096; size < refs_size; size += 4096) {
                allocate_frame(); 
            }
            for (uint64_t i = 0; i < num_frames; i++) {
                frame_refs[i] = 1;
            }
        }
    }
}

uint64_t allocate_frame(void) {
    while (current_region < usable_region_count) {
        struct limine_memmap_entry *r = usable_regions[current_region];
        uint64_t start = r->base;
        uint64_t end   = r->base + r->length;

        if (next_frame_addr < start)
            next_frame_addr = start;

        uint64_t frame = (next_frame_addr + (FRAME_SIZE - 1)) & ~(FRAME_SIZE - 1);

        if (frame + FRAME_SIZE <= end) {
            next_frame_addr = frame + FRAME_SIZE;
	    uint64_t *v = (uint64_t *)PHYS_TO_VIRT(frame);
	    for(int i = 0; i < 512; i++) {
		    v[i] = 0;
	    }
        if (frame_refs && (frame / 4096) < (max_phys_global / 4096)) {
            frame_refs[frame / 4096] = 1;
        }
        return frame;
        }

        // region ini habis → pindah ke region berikutnya
        current_region++;
    }

    return 0; // out of memory
}

block_t *heap_head = NULL;

void *malloc(uint64_t size, uint64_t alignment) {
	if(size == 0) return NULL;
    lock_malloc();

	while(current_region < usable_region_count) {
		struct limine_memmap_entry *r = usable_regions[current_region];
		uint64_t start = r->base;
		uint64_t end = r->base + r->length;

		if(next_frame_addr < start) 
		next_frame_addr = start;

	// Calculate aligned address for the DATA area (after block header)
	// We need: [block_t header][aligned data area]
	uint64_t total_size = sizeof(block_t) + size;
	uint64_t frame = (next_frame_addr + (alignment - 1)) & ~(alignment - 1);
	
	// Check if alignment pushed us past the block header alignment
	// Ensure there's room for the header before the aligned data
	uint64_t data_ptr = frame + sizeof(block_t);
	if ((data_ptr & (alignment - 1)) != 0) {
		uint64_t aligned_data = (data_ptr + alignment - 1) & ~(alignment - 1);
		frame = aligned_data - sizeof(block_t);
	}

	block_t *ptr = heap_head;

	while(ptr != NULL) {
		if(ptr->free == true && ptr->size >= size) {
            uint64_t data_ptr = (uint64_t)(ptr + 1);
            if ((data_ptr & (alignment - 1)) == 0) {
                ptr->free = false;  // Mark as used
                // Zero out the reused block
                uint8_t *v = (uint8_t *)(ptr + 1);
                for(uint64_t i = 0; i < ptr->size; i++) {
                    v[i] = 0;
                }
                unlock_malloc();
                return (void *)v;
            }
		}
		ptr = ptr->next;
	}

	if(frame + total_size <= end) {
		next_frame_addr = frame + total_size;
		
		uint8_t *v = (uint8_t *)PHYS_TO_VIRT(frame);
		for(uint64_t i = 0; i < total_size; i++) {
			v[i] = 0;
		}

		block_t *block = NULL;
		if(heap_head == NULL) {
			heap_head = (block_t *)v;
			heap_head->size = size;
			heap_head->free = false;
			heap_head->next = NULL;
			block = heap_head;
		}
		else {
			block_t *ptr_head = heap_head;
			while(ptr_head->next != NULL) {
				ptr_head = ptr_head->next;
			}	
			ptr_head->next = (block_t *)v;
			ptr_head->next->size = size;
			ptr_head->next->free = false;
			ptr_head->next->next = NULL;
			block = ptr_head->next;
		}
		uint64_t *res = (void *)(block + 1);
        unlock_malloc();
        return res;
	}
	current_region++;
}
    unlock_malloc();
	return NULL;
}

void free(void *ptr) {
    if (!ptr) return;
    lock_malloc();
	block_t *ptr_block = (block_t *)ptr;
	block_t *real_ptr = ptr_block - 1;
	real_ptr->free = true;
    unlock_malloc();
}

void inc_frame_ref(uint64_t phys) {
    if (frame_refs && (phys / 4096) < (max_phys_global / 4096)) {
        __atomic_add_fetch(&frame_refs[phys / 4096], 1, __ATOMIC_SEQ_CST);
    }
}

void dec_frame_ref(uint64_t phys) {
    if (frame_refs && (phys / 4096) < (max_phys_global / 4096)) {
        __atomic_sub_fetch(&frame_refs[phys / 4096], 1, __ATOMIC_SEQ_CST);
    }
}

uint16_t get_frame_ref(uint64_t phys) {
    if (frame_refs && (phys / 4096) < (max_phys_global / 4096)) {
        return __atomic_load_n(&frame_refs[phys / 4096], __ATOMIC_SEQ_CST);
    }
    return 1;
}