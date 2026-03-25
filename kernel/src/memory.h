#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef struct block {
	uint64_t size;
	bool free;
	struct block *next;
} block_t;

void frame_allocator_init(void);
uint64_t allocate_frame(void);
void inc_frame_ref(uint64_t phys);
void dec_frame_ref(uint64_t phys);
uint16_t get_frame_ref(uint64_t phys);
void *malloc(uint64_t size, uint64_t alignment);
void free(void *ptr);