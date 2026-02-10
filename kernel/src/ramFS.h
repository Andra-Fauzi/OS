#pragma once
#include <stdint.h>
#include "memory.h"

#define DIRECTORY 0x1
#define FILE 0x2

typedef struct ram_node {
	char name[128];
	uint32_t length;
	uint32_t flags;
	char *content;
	
	struct ram_node *parent;
	struct ram_node *children;
	struct ram_node *next;

} ram_node_t;
