#pragma once
#include <stdint.h>
#include "apic.h"
#include "keyboard.h"
void set_idt_entry(int n, void* handler, uint16_t selector, uint16_t type_attr);
void idt_init();
