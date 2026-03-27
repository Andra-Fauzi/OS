section .data
    hello db "Hello from sbrk!", 10
    hello_len equ $ - hello

section .text
global _start

_start:
    ; get current break: sbrk(0)
    mov rax, 12      ; syscall: sbrk
    xor rdi, rdi     ; increment = 0
    int 128
    mov r8, rax      ; save current break in r8

    ; allocate 64 bytes: sbrk(64)
    mov rax, 12
    mov rdi, 64
    int 128
    mov rsi, rax     ; rsi = returned old break (destination)

    ; copy string from .data to allocated memory
    mov rcx, hello_len
    lea rdi, [rel hello]
copy_loop:
    mov al, [rdi]
    mov [rsi], al
    inc rdi
    inc rsi
    dec rcx
    jnz copy_loop

    ; original dest pointer was in rax (returned by sbrk)
    ; but we overwrote rsi; use r8 (sbrk(0) original) and compute pointer:
    ; The pointer we want to write is r8 + (rax - r8) == rax, so reuse rax

    ; write(1, rax, hello_len)
    mov rdx, hello_len
    mov rdi, 1
    mov rsi, rax
    mov rax, 1
    int 128

    ; exit(0)
    mov rdi, 0
    mov rax, 60
    int 128
