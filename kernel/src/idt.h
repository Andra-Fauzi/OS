#pragma once
#include <stdint.h>
#include "apic.h"
#include "keyboard.h"
void set_idt_entry(int n, void* handler);
void idt_init();
