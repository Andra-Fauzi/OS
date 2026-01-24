#pragma once
#include "apic.h"

char keyboard_getchar();
void init_keyboard();
__attribute__((interrupt))
void keyboard_callback(void *frame);