; jump_to_usermode(uint64_t entry_point, uint64_t user_stack)
global jump_to_usermode
jump_to_usermode:
    ; rdi = entry_point (RIP)
    ; rsi = user_stack (RSP)

    mov ax, 0x23      ; Selector User Data (0x20 | 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Susun stack frame untuk IRETQ
    push 0x23         ; SS (User Data Selector)
    push rsi          ; RSP (User Stack)
    pushfq            ; RFLAGS
    
    ; Aktifkan Interrupt di RFLAGS (bit 9)
    pop rax
    or rax, 0x200
    push rax

    push 0x1B         ; CS (User Code Selector: 0x18 | 3)
    push rdi          ; RIP (Entry Point)

    iretq             ; BOOM! Kita sekarang di Usermode.