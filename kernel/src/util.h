#pragma once
#include <stdint.h>
#include <stddef.h>

int strlen(char *str);
char *strpbrk(char *str, char *delim);
char *strtok(char *str, const char *delim);
int strcmp(const char *s1, const char *s2);
void strcpy(char *destination, char *source);
char *strrchr(const char *str, int c);

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
