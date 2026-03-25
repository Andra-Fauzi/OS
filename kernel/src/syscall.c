#include "syscall.h"

extern process_t *running_process;

// PROCESS

void sys_exit(struct interrupt_frame *frame) {
    printf("\nUsermode Program Exited with status: %d\n", (int)frame->rbx);
    // Untuk sekarang, kita tahan CPU atau bisa melakukan yield/penghancuran thread
    // switch_context(frame);
    remove_process();
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
    frame->rax = result;
    return;
}

void sys_write(struct interrupt_frame *frame) {
    int result = vfs_write(frame->rdi, (void *)frame->rsi, frame->rdx);
    frame->rax = result;
    return;
}

void sys_open(struct interrupt_frame *frame) {
    const char *user_path = (const char *)frame->rdi;
    char final_path[VFS_PATH_LENGTH];
    if (user_path[0] == '/') {
        // Absolute path — use directly, no cwd prefix needed
        strcpy(final_path, user_path);
    } else {
        // Relative path — prepend cwd
        strcpy(final_path, running_process->cwd);
        strcat(final_path, user_path);
    }
    int result = vfs_open((const char*)final_path, frame->rsi);
    frame->rax = result;
    return;
}

void sys_close(struct interrupt_frame *frame) {
    int result = vfs_close(frame->rdi);
    frame->rax = result;
    return;
}

void sys_lseek(struct interrupt_frame *frame) {
    int result = vfs_seek(frame->rdi, frame->rsi);
    frame->rax = result;
    return;
}

// FILE INFO

void sys_stat(struct interrupt_frame *frame) {
    int result = vfs_stat((const char *)frame->rdi, (struct stat *)frame->rsi);
    frame->rax = result;
    return;
}

void sys_fstat(struct interrupt_frame *frame) {
    int result = vfs_fstat(frame->rdi, (struct stat *)frame->rsi);
    frame->rax = result;
    return;
}

// MEMORY

void sys_mmap(struct interrupt_frame *frame) {
    
}

// FILE DESCRIPTOR OPS

void sys_dup(struct interrupt_frame *frame) {
    int result = vfs_dup(frame->rdi);
    frame->rax = result;
    return;
}

void sys_dup2(struct interrupt_frame *frame) {
    int result = vfs_dup2(frame->rdi, frame->rsi);
    frame->rax = result;
    return;
}

void sys_pipe(struct interrupt_frame *frame) {
    int result = vfs_pipe((int *)frame->rdi);
    frame->rax = result;
    return;
}

// DIRECTORIES

void sys_chdir(struct interrupt_frame *frame) {
    const char *user_path = (const char *)frame->rdi;
    char final_path[128];
    if (user_path[0] == '/') {
        // Absolute path — use directly, no cwd prefix needed
        strcpy(final_path, user_path);
    } else {
        // Relative path — prepend cwd
        strcpy(final_path, running_process->cwd);
        strcat(final_path, user_path);
    }

    vfs_inode_t *inode = vfs_lookup(final_path, O_RDONLY);
    if(!inode){
        frame->rax = -1;
        return;
    }
    // 1 in this mean directory
    if(!(inode->type == 1)) {
        frame->rax = -1;
        return;
    }

    strcpy(running_process->cwd, final_path);
    vfs_free_inode(inode);
    frame->rax = 0;
    return;
}

void sys_getcwd(struct interrupt_frame *frame) {
    size_t len = strlen(running_process->cwd);
    if(len + 1 > frame->rsi) {
        frame->rax = -1;
        return;
    }

    strcpy((char *)frame->rdi, running_process->cwd);
    frame->rax = 0;
    return;
}

// SIGNALS

void sys_sigaction(struct interrupt_frame *frame) {

}

void sys_kill(struct interrupt_frame *frame) {
    
}

// TIME

void sys_nanosleep(struct interrupt_frame *frame) {

}

void sys_clock_gettime(struct interrupt_frame *frame) {

}

// FILESYSTEM CHANGES



// DEVICE / TERMINAL

void sys_ioctl(struct interrupt_frame *frame) {
    int result = vfs_ioctl(frame->rdi, frame->rsi, (void*)frame->rdx);
    frame->rax = result;
    return;
}

void sys_fcntl(struct interrupt_frame *frame) {
    int result = vfs_fcntl(frame->rdi, frame->rsi, frame->rdx);
    frame->rax = result;
    return;
}