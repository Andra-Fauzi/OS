#pragma once
#include <stdint.h>
#include <stdarg.h>
#include "main.h"
void write_terminal(char c);
void print_str(char *str);
void print_hex(uint64_t n);
void print_int(int64_t val);
void printf(char *str, ...);
void clear_screen();
void terminal_scroll();
void print_uint(uint64_t val);
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
