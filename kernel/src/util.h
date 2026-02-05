#pragma once
#include <stdint.h>
#include <stddef.h>

int strlen(char *str);
char *strpbrk(char *str, char *delim);
char *strtok(char *str, const char *delim);
