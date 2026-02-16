#include "syscall.h"

void sys_exit(struct interrupt_frame *frame) {
    printf("Usermode Program Exited with status: %d\n", (int)frame->rbx);
    // Untuk sekarang, kita tahan CPU atau bisa melakukan yield/penghancuran thread
    // switch_thread(frame);
    remove_thread();
    // kill_running_thread(frame);
    // asm volatile("int $0x40");
    while(1) {
        // printf("STILL RUNNING BROOOOOO\n");
        // asm volatile("int $0x40");
        asm volatile("hlt");
    }
}

void sys_read(struct interrupt_frame *frame) {
    int result = vfs_read(frame->rdi, (void *)frame->rsi, frame->rdx);
}

void sys_write(struct interrupt_frame *frame) {
    int result = vfs_write(frame->rdi, (void *)frame->rsi, frame->rdx);
}