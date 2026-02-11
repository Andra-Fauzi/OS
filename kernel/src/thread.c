#include "thread.h"
#include "ehci.h"
#include "vfs.h"
thread_t *main_thread = NULL;
thread_t *running_task = NULL;

static void enable_sse() {
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((uint64_t)1 << 2); // clear EM
    cr0 |= (1 << 1);  // set MP
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // set OSFXSR
    cr4 |= (1 << 10); // set OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

static uint8_t initial_fpu_state[512] __attribute__((aligned(16)));

static void init_fpu_state_buffer() {
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : : "m"(initial_fpu_state));
}

void first() {
    running_task->lock = true;
    printf("Testing terminal\n");
    int fd = vfs_open("/dev/tty", O_RDWR);
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
    running_task->lock = false;
    printf("first task\n");
    while(1) {
        asm volatile("sti");
        running_task->lock = true;
        input_mouse_ehci();
        printf("first task\n");
        running_task->lock = false;
        asm volatile("hlt");
    }
}

void second() {
    running_task->lock = true;
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
    running_task->lock = false;
    printf("second task\n");
    while(1) {
        asm volatile("sti");
        running_task->lock = true;
        printf("second task\n");
        running_task->lock = false;
        asm volatile("hlt");
    }
}

void init_thread() {
    enable_sse();
    init_fpu_state_buffer();
    main_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    memcpy(main_thread->fpu_state, initial_fpu_state, 512);
    thread_t *first_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    create_thread(first_thread, first);
    thread_t *second_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    create_thread(second_thread, second);
    main_thread->next = first_thread;
    first_thread->next = second_thread;
    second_thread->next = main_thread;
    running_task = main_thread;
}

void create_thread(thread_t *thread, void (*func)()) {
    thread->frame.r15 = 0;
    thread->frame.r14 = 0;
    thread->frame.r13 = 0;
    thread->frame.r12 = 0;
    thread->frame.r11 = 0;
    thread->frame.r10 = 0;
    thread->frame.r9 = 0;
    thread->frame.r8 = 0;
    thread->frame.rsi = 0;
    thread->frame.rdi = 0;
    thread->frame.rbp = 0;
    thread->frame.rdx = 0;
    thread->frame.rcx = 0;
    thread->frame.rbx = 0;
    thread->frame.rax = 0;
    thread->frame.rip = (uint64_t)func;
    thread->frame.cs = 0x28;
    thread->frame.rflags = 0x282;
    // Increase stack size to 8KB
    thread->frame.rsp = (uint64_t)malloc(8192, 16) + 8192;
    thread->frame.ss = 0x30;
    thread->lock = false;
    memcpy(thread->fpu_state, initial_fpu_state, 512);
}

void switch_thread(struct interrupt_frame *frame) {
    if (running_task == NULL) {
        return;
    }
    if(running_task->lock) {
        return;
    }
    __asm__ volatile("fxsave %0" : : "m"(running_task->fpu_state));
    running_task->frame = *frame;
    running_task = running_task->next;
    *frame = running_task->frame;
    __asm__ volatile("fxrstor %0" : : "m"(running_task->fpu_state));
}