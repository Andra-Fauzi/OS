section .bss
    buffer resb 128

section .data
    message db 'Hello, ANDRA!', 0xa ; The message string and a newline character (0xa)
    msg_len equ $ - message         ; Calculate the length of the message
    path db '/COBA.TXT', 0 ; test
    data db 'halo andra ganteng sigma ohio', 0xa
    data_len equ $ - data
    path2 db '/ELF_TEST', 0 ; test read
    path3 db '/S_TEST', 0 ; test execve 
    path4 db '/SBRK', 0 ; test sbrk

section .text
    global _start                   ; Declare _start as the program entry point

_start:

    mov rax, 2 ; syscall number 2 for sys_open
    mov rdi, path
    mov rsi, 7 ; flags = 7 (O_CREAT | O_RDWR)
    int 128

    mov rdi, rax ; mov the file descriptor to rdi
    mov rax, 1 ; syscall number 1 for sys_write
    mov rsi, data   
    mov rdx, data_len
    int 128

    ; --- lseek to beginning of file before reading ---
    mov rax, 8 ; syscall number 8 for sys_lseek
    ; rdi is already the file descriptor
    mov rsi, 0 ; offset 0
    int 128

    mov rax, 0 ; syscall number 0 for sys_read
    mov rsi, buffer
    mov rdx, 128
    int 128

    ; --- write syscall ---
    mov rax, 1                      ; syscall number 1 for sys_write
    mov rdi, 1                      ; file descriptor 1 for stdout
    mov rsi, buffer                ; address of the message buffer
    mov rdx, 128                ; number of bytes to write
    int 128                         ; invoke the kernel

    ;jmp $

    mov rax, 59
    lea rdi, path4
    int 128

    ; --- exit syscall ---
    mov rax, 60                     ; syscall number 60 for sys_exit
    xor rdi, rdi                    ; exit status 0 (success)
    int 128                   ; invoke the kernel
