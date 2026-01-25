#pragma once
#include "apic.h"
#include "idt.h"
#include "memory.h"
#include "paging.h"
#include "keyboard.h"
#include "fat.h"

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
