#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct block {
	uint64_t size;
	struct block *next;
} block_t;

void frame_allocator_init(void);
uint64_t allocate_frame(void);
void *malloc(uint64_t size, uint64_t alignment);
