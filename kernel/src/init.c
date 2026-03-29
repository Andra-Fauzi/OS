#include "init.h"
#include "cpu.h"
#include "main.h"
#include "gdt.h"
#include "terminal.h"
#include "ahci.h"
#include "vfs.h"
#include "vfs_terminal.h"
#include "fat_adapter.h"
#include "util.h"
#include <stddef.h>

extern volatile uint32_t *framebuffer_ptr;
extern volatile uint64_t framebuffer_pitch;
extern volatile uint64_t framebuffer_height;
extern volatile uint64_t framebuffer_width;

static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void check_limine_revision(volatile uint64_t* revision) {
    if (LIMINE_BASE_REVISION_SUPPORTED(revision) == false) {
        hcf();
    }
}

void get_framebuffer(struct limine_framebuffer_request* request) {
    if (request->response == NULL || request->response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = request->response->framebuffers[0];
    framebuffer_ptr = framebuffer->address;
    framebuffer_pitch = framebuffer->pitch;
    framebuffer_width = framebuffer->width;
    framebuffer_height = framebuffer->height;
}

void sys_init() {
    cpu_init();
    frame_allocator_init();
    print_str("frame allocator initialized\n");

    asm volatile("cli");
    gdt_init();
    print_str("GDT Loaded\n");
    
    idt_init();
    print_str("IDT Loaded\n");

    isr_install();
    print_str("ISR Installed\n");

    pic_disable();
    print_str("PIC Disabled\n");

    lapic_enable();
    print_str("APIC Enabled\n");

    lapic_timer_init();
    print_str("Timer Initialized\n");

    init_interrupt();
    print_str("Interrupts Initialized\n");

    init_keyboard();
    print_str("Keyboard Initialized\n");

    init_thread();
    print_str("Thread Initialized\n");
}

void drivers_init() {
    // setup_ehci();
    // print_str("EHCI Initialized\n");

    // setup_mouse_ehci();
    // print_str("Mouse EHCI Initialized\n");
    
    // input_mouse_ehci();

    printf("Setup AHCI\n");
    setup_ahci();
}

void vfs_setup_mounts() {
    fat_init();
    vfs_init();
    
    fs_operations_t *fat_ops = fat_get_operations();
    vfs_mount("/", "disk0", "fat32", fat_ops);
    // vfs_mount("/dev/sda", "/", "fat32", fat_ops);
    vfs_mount("/dev/tty", "terminal", "tty", vfs_terminal_get_ops());
}
