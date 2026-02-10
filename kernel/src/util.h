#pragma once
#include <stdint.h>
#include <stddef.h>

int strlen(char *str);
char *strpbrk(char *str, char *delim);
char *strtok(char *str, const char *delim);
int strcmp(const char *s1, const char *s2);
void strcpy(char *destination, char *source);
char *strrchr(const char *str, int c);
