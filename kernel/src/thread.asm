extern switch_thread
global switch_context
switch_context:
    ; save general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; optional (if GS base used per-cpu) (and if it have usermode support)
    ; swapgs

    mov rdi, rsp        ; arg1 = pointer ke interrupt frame
    call switch_thread

    ; swapgs            ; if it used before

    ; restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16         ; dump error code + int number
    iretq