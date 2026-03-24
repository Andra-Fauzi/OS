#include "syscall.h"

// PROCESS

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

void sys_fork(struct interrupt_frame *frame) {

}

void sys_execve(struct interrupt_frame *frame) {

}

void sys_waitpid(struct interrupt_frame *frame) {
    
}

// FILE I/O

void sys_read(struct interrupt_frame *frame) {
    int result = vfs_read(frame->rdi, (void *)frame->rsi, frame->rdx);
}

void sys_write(struct interrupt_frame *frame) {
    int result = vfs_write(frame->rdi, (void *)frame->rsi, frame->rdx);
}

void sys_open(struct interrupt_frame *frame) {

}

void sys_close(struct interrupt_frame *frame) {
    
}

// FILE INFO

void sys_stat(struct interrupt_frame *frame) {

}

void sys_fstat(struct interrupt_frame *frame) {
    
}

// MEMORY

void sys_mmap(struct interrupt_frame *frame) {

}

// FILE DESCRIPTOR OPS

void sys_dup(struct interrupt_frame *frame) {

}

void sys_dup2(struct interrupt_frame *frame) {
    
}

void sys_pipe(struct interrupt_frame *frame) {

}

// DIRECTORIES

void sys_chdir(struct interrupt_frame *frame) {

}

void sys_getcwd(struct interrupt_frame *frame) {

}

// SIGNALS

void sys_sigaction(struct interrupt_frame *frame) {

}

void sys_kill(struct interrupt_frame *frame) {
    
}

// TIME

void sys_nanosleep(struct interrupt_frame *frame) {

}

void sys_clock_getttime(struct interrupt_frame *frame) {

}

// FILESYSTEM CHANGES



// DEVICE / TERMINAL

void sys_ioctl(struct interrupt_frame *frame) {

}

void sys_fcntl