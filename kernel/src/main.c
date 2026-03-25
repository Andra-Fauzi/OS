#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "main.h"
#include "init.h"
#include "tests.h"
#include "util.h"
#include "terminal.h"
#include "process.h"
#include "vfs.h"

// Limine base revision
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

// Framebuffer request
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// Limine start/end markers
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Globals
volatile uint32_t *framebuffer_ptr;
volatile uint64_t framebuffer_pitch;
volatile uint64_t framebuffer_height;
volatile uint64_t framebuffer_width;

extern thread_t *running_thread;

// Example Tasks
void first_task() {
    printf("First task started\n");
    while(1) {
        asm volatile("sti");
        // printf("first thread\n");
        // input_mouse_ehci();
        asm volatile("hlt");
    }
}

void second_task() {
    printf("Shell task started\n");
    run_diagnostic_tests();

    int fd = vfs_open("/dev/tty", O_RDWR);
    if (fd < 0) {
        printf("Shell: Failed to open /dev/tty\n");
        while(1) asm volatile("hlt");
    }

    extern void total_thread();
    int root_fd = vfs_open("/", O_RDWR);
    char buf[128];
    while(1) {
        printf("second thread\n");
        total_thread();
        total_process();
        vfs_write(fd, "andra-os> ", 10);
        int bytes = vfs_read(fd, buf, 127);
        if (bytes > 0) {
            buf[bytes] = '\0';
            vfs_write(fd, "You typed: ", 11);
            vfs_write(fd, buf, bytes);
            vfs_write(fd, "\n", 1);
        }
        printf("readdir '/'\n");
        vfs_dirent_t dirent;
        int result = vfs_readdir(root_fd, &dirent); 
        printf("result is %d\nfd is %d\n", (uint64_t)result, (uint64_t)root_fd);
        while(result > -1) {
            printf("name: %s, ino: %d\n", dirent.name, dirent.ino);
            result = vfs_readdir(root_fd, &dirent);
        }
        vfs_seek(root_fd, 0);
    }
}

// Kernel entry point
void kmain(void) {
    check_limine_revision(limine_base_revision);
    get_framebuffer(&framebuffer_request);

    // Core System Init
    sys_init();
    
    // Driver Init
    drivers_init();
    
    // File System Setup
    vfs_setup_mounts();

    asm volatile("sti");
    print_str("Kernel initialization complete. Interrupts are ON.\n");

    // Start Example Threads
    process_t *p1 = (process_t *)malloc(sizeof(process_t), 16);
    create_process(p1, first_task);
    add_process(p1);

    process_t *p2 = (process_t *)malloc(sizeof(process_t), 16);
    create_process(p2, second_task);
    add_process(p2); 

    extern void test_elf();

    process_t *p3 = (process_t *)malloc(sizeof(process_t), 16);
    create_process(p3, test_elf);
    add_process(p3);

    extern uint32_t PID_TOTAL;

    asm volatile("sti");

    // run_diagnostic_tests();

    // Main loop
    while(1) {
        // printf("main thread\n");
        // printf("total thread %d\n", PID_TOTAL);
	    asm volatile("hlt");
    }
}
