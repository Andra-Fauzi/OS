#include "syscall.h"
#include "elf.h"

extern process_t *running_process;
extern thread_t *running_thread;
extern process_t *main_process;
extern uint32_t PID_TOTAL;

// PROCESS

void sys_exit(struct interrupt_frame *frame) {
    printf("\nUsermode Program Exited with status: %d\n", (uint64_t)frame->rdi);
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

#include "paging.h"

void copy_page_tables(uint64_t *child_pml4, uint64_t *parent_pml4) {
    // Copy kernel space (top half, entries 256-511)
    for (int i = 256; i < 512; i++) {
        child_pml4[i] = parent_pml4[i];
    }

    // COW copy user space
    for (int i = 0; i < 256; i++) {
        if (parent_pml4[i] & PTE_PRESENT) {
            uint64_t *parent_pdpt = (uint64_t *)PHYS_TO_VIRT(parent_pml4[i] & PTE_ADDR_MASK);
            uint64_t *child_pdpt = get_next_level(child_pml4, i);

            for (int j = 0; j < 512; j++) {
                if (parent_pdpt[j] & PTE_PRESENT) {
                    uint64_t *parent_pd = (uint64_t *)PHYS_TO_VIRT(parent_pdpt[j] & PTE_ADDR_MASK);
                    uint64_t *child_pd = get_next_level(child_pdpt, j);

                    for (int k = 0; k < 512; k++) {
                        if (parent_pd[k] & PTE_PRESENT) {
                            if (parent_pd[k] & PTE_HUGE) {
                                child_pd[k] = parent_pd[k]; 
                                continue;
                            }
                            uint64_t *parent_pt = (uint64_t *)PHYS_TO_VIRT(parent_pd[k] & PTE_ADDR_MASK);
                            uint64_t *child_pt = get_next_level(child_pd, k);

                            for (int l = 0; l < 512; l++) {
                                if (parent_pt[l] & PTE_PRESENT) {
                                    // COW LOGIC
                                    parent_pt[l] &= ~PTE_WRITABLE;
                                    parent_pt[l] |= PTE_COW;
                                    
                                    child_pt[l] = parent_pt[l];

                                    inc_frame_ref(parent_pt[l] & PTE_ADDR_MASK);
                                    
                                    // Invalidate TLB for parent
                                    uint64_t vaddr = ((uint64_t)i << 39) | ((uint64_t)j << 30) | ((uint64_t)k << 21) | ((uint64_t)l << 12);
                                    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

mm_struct_t* clone_address_space(mm_struct_t *parent) {
    mm_struct_t *child = malloc(sizeof(mm_struct_t), 16);
    child->pml4 = PHYS_TO_VIRT(allocate_frame());

    copy_page_tables(child->pml4, parent->pml4);

    return child;
}

void sys_fork(struct interrupt_frame *frame) {
    asm volatile("cli");
    
    process_t *child = malloc(sizeof(process_t), 16);
    memcpy(child, running_process, sizeof(process_t));
    
    // Allocate new mm_struct for the child instead of sharing the parent's pointer
    child->mm = clone_address_space(running_process->mm);
    
    child->pid = PID_TOTAL++;
    
    thread_t *child_thread = malloc(sizeof(thread_t), 16);
    memcpy(child_thread, running_thread, sizeof(thread_t));

    // Fix: Allocate unique kernel stack for the child
    child_thread->stack_base = malloc(8192, 16);
    memcpy(child_thread->fpu_state, running_thread->fpu_state, 512);
    memcpy(&child_thread->frame, frame, sizeof(struct interrupt_frame));
    memcpy(child_thread->stack_base, running_thread->stack_base, 8192);
    // child_thread->frame.rsp = (uint64_t)child_thread->stack_base + 8192;

    // Update child thread frame with the current syscall context
    // child_thread->frame = *frame;
    
    child->threads = child_thread;
    child_thread->next = child_thread;
    
    
    add_process(child);
    frame->rax = child->pid;     // Parent returns child PID
    child_thread->frame.rax = (uint64_t)0; // Child returns 0
    
    // printf("Forked new process with PID: %d\n", child->pid);
    // printf("Child thread frame rax: %d\n rbx: %d\n rcx: %d\n rdx: %d\n rsi: %d\n rdi: %d\n", 
    //     (uint64_t)child_thread->frame.rax, (uint64_t)child_thread->frame.rbx, (uint64_t)child_thread->frame.rcx, 
    //     (uint64_t)child_thread->frame.rdx, (uint64_t)child_thread->frame.rsi, (uint64_t)child_thread->frame.rdi);
    asm volatile("sti");
}

void sys_execve(struct interrupt_frame *frame) {
    const char *path = (char *)frame->rdi;

    // Create new address space
    mm_struct_t *new_mm = create_empty_mm();

    // Load ELF into the new address space
    // We pass the new PML4 so that load_elf maps segments there
    ELF_HEADER_t *elf_header = load_elf(path, new_mm->pml4);
    if(elf_header == NULL) {
        // TODO: cleanup new_mm
        frame->rax = -1;
        return;
    }

    // Switch the process to the new address space
    // In a real OS, we would free the old mm here.
    running_process->mm = new_mm;
    load_cr3(VIRT_TO_PHYS(new_mm->pml4));

    // Allocate and map a fresh user stack
    uint64_t stack_phys = allocate_frame();
    uint64_t stack_virt = 0x70000000000;
    map_page(new_mm->pml4, stack_virt, stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    // Set up the interrupt frame to return to the new entry point
    frame->rip = (uint64_t)elf_header->entry_point;
    frame->rsp = stack_virt + 4096;
    frame->rax = 0;

    // Clean up the temporary header
    free(elf_header);
}

void sys_waitpid(struct interrupt_frame *frame) {
    printf("sys_waitpid called with PID: %d\n", (uint64_t)frame->rdi);
    while(1) {
        int found = 0;
        asm volatile("cli");
        process_t *prev = main_process;
        if (prev == NULL) {
            asm volatile("sti");
            frame->rax = -1;
            return;
        }

        while (prev->next != main_process) {
            if (prev->pid == frame->rdi) {
                found = 1;
                break;
            }
            prev = prev->next;
        }
        if (prev->pid == frame->rdi) {
            found = 1;
        }
        asm volatile("sti");

        if (!found) {
            frame->rax = -1; // No such child
            return;
        }

        // Return PID of the child that is found. 
        // For now, simpler implementation just to unblock the caller.
        // frame->rax = frame->rdi;
        // return;
    }
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