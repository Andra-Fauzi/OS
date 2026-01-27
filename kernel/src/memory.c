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
	}
	if(usable_region_count > 0) {
		next_frame_addr = usable_regions[0]->base;
		if(next_frame_addr < 0x100000) {
			next_frame_addr = 0x100000;
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

        uint64_t frame = (next_frame_addr + FRAME_SIZE - 1) & ~(FRAME_SIZE - 1);

        if (frame + FRAME_SIZE <= end) {
            next_frame_addr = frame + FRAME_SIZE;
	    uint64_t *v = (uint64_t *)PHYS_TO_VIRT(frame);
	    for(int i = 0; i < 512; i++) {
		    v[i] = 0;
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

	while(current_region < usable_region_count) {
		struct limine_memmap_entry *r = usable_regions[current_region];
		uint64_t start = r->base;
		uint64_t end = r->base + r->length;

		if(next_frame_addr < start) 
			next_frame_addr = start;

		uint64_t frame = (next_frame_addr + (alignment - 1)) & ~(alignment - 1);

		if(frame + size <= end) {
			next_frame_addr = frame + size;
			uint8_t *v = (uint8_t *)PHYS_TO_VIRT(frame);
			for(int i = 0; i < size; i++) {
				v[i] = 0;
			}
			return (void *)v;
		}
		current_region++;
	}
	return NULL;
}
