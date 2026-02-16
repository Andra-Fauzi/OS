section .data
    message db 'Hello, ANDRA!', 0xa ; The message string and a newline character (0xa)
    msg_len equ $ - message         ; Calculate the length of the message

section .text
    global _start                   ; Declare _start as the program entry point

_start:
    ; --- write syscall ---
    mov rax, 1                      ; syscall number 1 for sys_write
    mov rdi, 1                      ; file descriptor 1 for stdout
    mov rsi, message                ; address of the message buffer
    mov rdx, msg_len                ; number of bytes to write
    int 128                         ; invoke the kernel

    ;jmp $

    ; --- exit syscall ---
    mov rax, 60                     ; syscall number 60 for sys_exit
    xor rdi, rdi                    ; exit status 0 (success)
    int 128                   ; invoke the kernel
