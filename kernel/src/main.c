#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "main.h"
#include "terminal.h"
#include "ahci.h"
#include "paging.h"
#include "vfs.h"
#include "vfs_terminal.h"
#include "fat_adapter.h"
#include "gdt.h"

// Set the base revision to 4, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

#include "util.h"

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

volatile uint32_t *framebuffer_ptr;
volatile uint64_t framebuffer_pitch;
volatile uint64_t framebuffer_height;
volatile uint64_t framebuffer_width;

void first() {
    // running_thread->lock = true;
    // running_thread->lock = false;
    printf("first task\n");
    while(1) {
        asm volatile("sti");
        // running_thread->lock = true;
        input_mouse_ehci();
        printf("first task\n");
        // running_thread->lock = false;
        asm volatile("hlt");
    }
}

void second() {
    // running_thread->lock = true;
    // Test VFS Open/Write/Read (FAT)
    printf("Testing VFS...\n");
    int fd = vfs_open("/sia/TEST", O_CREAT | O_RDWR);
    if (fd >= 0) {
        printf("VFS Open Success: fd=%d\n", fd);
        char *msg = "Hello VFS World!";
        vfs_write(fd, msg, 16);
        vfs_close(fd);
        
        // Read back
        fd = vfs_open("/dev/sda/sia/TEST", O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            memset(buf, 0, 32);
            vfs_read(fd, buf, 32);
            printf("VFS Read Result: %s\n", buf);
            vfs_close(fd);
        }
    } else {
        printf("VFS Open Failed\n");
    }
    printf("Testing terminal\n");
    fd = vfs_open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        printf("VFS Open Success: fd=%d\n", fd);
        char *msg = "Hello VFS Terminal!\n";
        vfs_write(fd, msg, 21);
        vfs_close(fd);
        printf("Testing reading\n");
        fd = vfs_open("/dev/tty", O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            memset(buf, 0, 32);
            vfs_read(fd, buf, 32);
            printf("VFS Read Result: %s\n", buf);
            vfs_close(fd);
        } else {
            printf("VFS Open Failed\n");
        }
    } else {
        printf("VFS Open Failed\n");
    }
    printf("second task\n");
    while(1) {
        asm volatile("sti");
        // running_thread->lock = true;
        printf("second task\n");
        // running_thread->lock = false;
        asm volatile("hlt");
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    framebuffer_ptr = framebuffer->address;
    framebuffer_pitch = framebuffer->pitch;
    framebuffer_width = framebuffer->width;
    framebuffer_height = framebuffer->height;
    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    // for (size_t i = 0; i < 100; i++) {
    //     volatile uint32_t *fb_ptr = framebuffer->address;
    //     fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    // }

    // We're done, just hang...
    frame_allocator_init();
    print_str("frame allocator initialized\n");
    uint64_t *pml4 = get_pml4();
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

    setup_ehci();
    print_str("EHCI Initialized\n");

    setup_mouse_ehci();
    print_str("Mouse EHCI Initialized\n");

    asm volatile("sti");
    input_mouse_ehci();

    // Make sure everything is ready before opening the interrupt gate

    asm volatile("cli");
    printf("Setup AHCI\n");
    setup_ahci();
    
    if (sataport) {
        printf("Testing AHCI Read...\n");
        uint64_t buf_phys = allocate_frame();
        // Since we are now using PHYS_TO_VIRT in paging.h, we can use it here
        uint16_t *buf = (uint16_t*)PHYS_TO_VIRT(buf_phys);
        
        // Read 1 sector (512 bytes) from LBA 0
        uint8_t *buf2 = (uint8_t*)PHYS_TO_VIRT(buf_phys);
        bool success = ahci_read(sataport, 0, 0, 1, buf2);
        MBR_t *mbr = (MBR_t*)buf2;
        partition_entry_t *partition = (partition_entry_t*)mbr->partition_table;
        
        if(success) {
            printf("AHCI Read Success! Data:\n");
            for(int i=0; i<512; i++) {
                printf("%c ", (char)buf2[i]);
            }
            printf("\n");
            printf("Partition Table:\n");
            for(int i=0; i<4; i++) {
                printf("Partition %d:\n", i+1);
                printf("  Bootable: %d\n", partition[i].bootable);
                printf("  Type: %x\n", partition[i].type);
                printf("  Start Sector: %d\n", partition[i].LBA_start_sector);
                printf("  Total Sectors: %d\n", partition[i].total_sectors);
            }
        } else {
            printf("AHCI Read Failed\n");
        }
    } else {
        printf("No SATA port found for testing.\n");
        asm volatile("hlt");
    }
    
    printf("testing identify\n");
    identify(sataport);
    
    uint32_t total = 0;
    fat_init();
    
    // VFS Integration
    vfs_init();
    fs_operations_t *fat_ops = fat_get_operations();
    vfs_mount("/", "disk0", "fat32", fat_ops);
    vfs_mount("/dev/sda", "/", "fat32", fat_ops);
    
    // Terminal VFS
    vfs_mount("/dev/tty", "terminal", "tty", vfs_terminal_get_ops());
    
    asm volatile("sti");
    print_str("Interrupts are now ON\n");

    extern thread_t *running_thread;
    
    thread_t *first_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    create_thread(first_thread, first);
    thread_t *second_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    create_thread(second_thread, second);
    add_thread(first_thread);
    add_thread(second_thread);

    while(1) {
        running_thread->lock = true;
        // printf("main task\n");
        // input_mouse_ehci();
        running_thread->lock = false;
	    asm volatile("hlt");
    }


    hcf();
}
