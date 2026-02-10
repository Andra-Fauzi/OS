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

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

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

    init_keyboard();
    print_str("Keyboard Initialized\n");

    // Make sure everything is ready before opening the interrupt gate

    asm volatile("cli");
    printf("Setup AHCI\n");
    setup_ahci();

    asm volatile("sti");
    print_str("Interrupts are now ON\n");

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

    /*
    uint32_t table_value = FAT32_read(5);
    printf("table value %d\n", table_value);
    FAT32_write(5, 20);
    uint32_t new_table_value = FAT32_read(5);
    printf("new table value %d\n", new_table_value);
    */
    // extern uint32_t fat_size;
    // printf("fat size is %d\n", fat_size);

    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(fat_dir_entry_t));
    memset(&entry.file_name, ' ', 11);
    entry.file_name[0] = 'T';
    entry.file_name[1] = 'E';
    entry.file_name[2] = 'S';
    entry.file_name[3] = 'T';
    entry.attribute_file = 0x20;
    entry.first_cluster_low = 0;
    entry.first_cluster_high = 0;
    entry.size_file = 0;

    // create_entry("/", &entry);

    // write_data("/", &entry, "Hello World!", 11);
    // write_data("/", &entry, "Hello ANDRA!", 11);

    listing_root_dir_print();

    // VFS Integration
    vfs_init();
    fs_operations_t *fat_ops = fat_get_operations();
    vfs_mount("/", "disk0", "fat32", fat_ops);
    vfs_mount("/dev/sda", "/", "fat32", fat_ops);
    
    // Terminal VFS
    vfs_mount("/dev/tty", "terminal", "tty", vfs_terminal_get_ops());
    vfs_mount("/dev/tty", "/", "fat32", fat_ops);

    // Test VFS Terminal
    printf("Testing Terminal VFS (Type something and press Enter)...\n");
    int fd_term = vfs_open("/dev/tty", O_RDWR);
    if(fd_term >= 0) {
        char buf[128];
        memset(buf, 0, 128);
        vfs_write(fd_term, "Enter text: ", 12);
        
        int read_count = vfs_read(fd_term, buf, 127); // Leave room for null terminator
        if(read_count > 0) {
            // Remove newline if present for cleaner output
            if(buf[read_count-1] == '\n') buf[read_count-1] = '\0';
            
            vfs_write(fd_term, "You typed: ", 11);
            vfs_write(fd_term, buf, read_count);
            vfs_write(fd_term, "\n", 1);
        }
        vfs_close(fd_term);
    } else {
        printf("Failed to open terminal VFS\n");
    }

    // Test VFS Open/Write/Read (FAT)
    printf("Testing VFS...\n");
    int fd = vfs_open("/EFI/TEST", O_CREAT | O_RDWR);
    if (fd >= 0) {
        printf("VFS Open Success: fd=%d\n", fd);
        char *msg = "Hello VFS World!";
        vfs_write(fd, msg, 16);
        vfs_close(fd);
        
        // Read back
        fd = vfs_open("/dev/sda/EFI/TEST", O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            memset(buf, 0, 32);
            vfs_read(fd, buf, 32);
            printf("VFS Read: %s\n", buf);
            vfs_close(fd);
        }
    } else {
        printf("VFS Open Failed\n");
    }

    // volatile uint64_t a = 1;
    // volatile uint64_t b = 0;
    // a = a / b;          // 💥 trigger #DE

    // char *halo = (char *)malloc(sizeof(char) * 5, 4);
    // halo[0] = 'a';
    // halo[1] = 'n';
    // halo[2] = 'd';
    // halo[3] = 'r';
    // halo[4] = 'a';
    // printf("str: %s\n", halo);
    // printf("alamat: %x\n", &halo[0]);
    // free(halo);
    // char *sigma = (char *)malloc(sizeof(char) * 5, 32);
    // sigma[0] = 's';
    // sigma[1] = 'i';
    // sigma[2] = 'g';
    // sigma[3] = 'm';
    // sigma[4] = 'a';
    // printf("str: %s\n", sigma);
    // printf("alamat: %x\n", &sigma[0]);
    // printf("str: %s\n", halo);
    // printf("alamat: %x\n", &halo[0]);
    // something();

    // for(volatile uint32_t i = 0; i < 0xFFFFFFFF; i++);
    
    // setup_mouse();
    // while(1) {
    // }
        
    // printf("test check 1\n");
    // check_device_status();
    // setup_ehci();
    // init_OHCI();
    // printf("test check 2\n");
    // find_device_port_and_sign_address();
    // check_device_status();
    // setup_mouse_ehci();
    // setup_mouse();

    // while(1) {
    //     input_mouse_ehci();
    // }
    
    
    
    while(1) {
        // char c = keyboard_getchar();
        // if (c != -1) {
        //     print(c);
        // }
        // input_mouse();
	asm volatile("hlt");
    }

    hcf();
}
