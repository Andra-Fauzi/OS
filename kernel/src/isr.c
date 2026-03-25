#include "isr.h"
#include "paging.h"
#include "memory.h"
#include "util.h"

void isr_install() {
    set_idt_entry(0, (void *)isr0, 0x08, 0x8E);
    set_idt_entry(1, (void *)isr1, 0x08, 0x8E);
    set_idt_entry(2, (void *)isr2, 0x08, 0x8E);
    set_idt_entry(3, (void *)isr3, 0x08, 0x8E);
    set_idt_entry(4, (void *)isr4, 0x08, 0x8E);
    set_idt_entry(5, (void *)isr5, 0x08, 0x8E);
    set_idt_entry(6, (void *)isr6, 0x08, 0x8E);
    set_idt_entry(7, (void *)isr7, 0x08, 0x8E);
    set_idt_entry(8, (void *)isr8, 0x08, 0x8E);
    set_idt_entry(9, (void *)isr9, 0x08, 0x8E);
    set_idt_entry(10, (void *)isr10, 0x08, 0x8E);
    set_idt_entry(11, (void *)isr11, 0x08, 0x8E);
    set_idt_entry(12, (void *)isr12, 0x08, 0x8E);
    set_idt_entry(13, (void *)isr13, 0x08, 0x8E);
    set_idt_entry(14, (void *)isr14, 0x08, 0x8E);
    set_idt_entry(15, (void *)isr15, 0x08, 0x8E);
    set_idt_entry(16, (void *)isr16, 0x08, 0x8E);
    set_idt_entry(17, (void *)isr17, 0x08, 0x8E);
    set_idt_entry(18, (void *)isr18, 0x08, 0x8E);
    set_idt_entry(19, (void *)isr19, 0x08, 0x8E);
    set_idt_entry(20, (void *)isr20, 0x08, 0x8E);
    set_idt_entry(21, (void *)isr21, 0x08, 0x8E);
    set_idt_entry(22, (void *)isr22, 0x08, 0x8E);
    set_idt_entry(23, (void *)isr23, 0x08, 0x8E);
    set_idt_entry(24, (void *)isr24, 0x08, 0x8E);
    set_idt_entry(25, (void *)isr25, 0x08, 0x8E);
    set_idt_entry(26, (void *)isr26, 0x08, 0x8E);
    set_idt_entry(27, (void *)isr27, 0x08, 0x8E);
    set_idt_entry(28, (void *)isr28, 0x08, 0x8E);
    set_idt_entry(29, (void *)isr29, 0x08, 0x8E);
    set_idt_entry(30, (void *)isr30, 0x08, 0x8E);
    set_idt_entry(31, (void *)isr31, 0x08, 0x8E);
}

void isr_handler(struct interrupt_frame* frame) {
    if (frame->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        
        uint64_t err = frame->err;
        // PF_PRESENT = 1 (bit 0), PF_WRITE = 2 (bit 1)
        if ((err & 0x3) == 0x3) { // Present and Write fault
            uint64_t *pml4 = get_pml4();
            
            size_t pml4_idx = (cr2 >> 39) & 0x1FF;
            size_t pdpt_idx = (cr2 >> 30) & 0x1FF;
            size_t pd_idx   = (cr2 >> 21) & 0x1FF;
            size_t pt_idx   = (cr2 >> 12) & 0x1FF;
            
            if (pml4[pml4_idx] & PTE_PRESENT) {
                uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[pml4_idx] & PTE_ADDR_MASK);
                if (pdpt[pdpt_idx] & PTE_PRESENT) {
                    uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pdpt[pdpt_idx] & PTE_ADDR_MASK);
                    if (pd[pd_idx] & PTE_PRESENT) {
                        uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pd[pd_idx] & PTE_ADDR_MASK);
                        if (pt[pt_idx] & PTE_PRESENT) {
                            if (pt[pt_idx] & PTE_COW) {
                                uint64_t old_phys = pt[pt_idx] & PTE_ADDR_MASK;
                                uint16_t refs = get_frame_ref(old_phys);
                                
                                if (refs > 1) {
                                    uint64_t new_phys = allocate_frame();
                                    memcpy(PHYS_TO_VIRT(new_phys), PHYS_TO_VIRT(old_phys), 4096);
                                    
                                    dec_frame_ref(old_phys);
                                    pt[pt_idx] = (pt[pt_idx] & ~PTE_ADDR_MASK) | new_phys;
                                }
                                
                                pt[pt_idx] |= PTE_WRITABLE;
                                pt[pt_idx] &= ~PTE_COW;
                                
                                asm volatile("invlpg (%0)" :: "r"(cr2) : "memory");
                                return;
                            }
                        }
                    }
                }
            }
        }
        
        printf("Page Fault at %x\n", cr2);
        printf("Error Code %x\n", frame->err);
        while(1) asm volatile("hlt");
    }

    printf("Interrupt %d\n", frame->int_no);
    printf("RIP : %x\n", frame->rip);
    while(1) asm volatile("hlt");
}
