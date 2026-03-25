section .data
    parent_msg db "parent", 10
    parent_len equ $ - parent_msg

    child_msg db "child", 10
    child_len equ $ - child_msg

section .text
global _start

_start:
    ; syscall: fork()
    mov rax, 57        ; sys_fork
    int 128           ; your syscall interrupt

    ; result in rax
    ;cmp rax, 0
    jmp exit

parent:
    ; write(1, "parent\n", len)
    mov rax, 1         ; sys_write
    mov rdi, 1         ; fd = stdout
    mov rsi, parent_msg
    mov rdx, parent_len
    int 128
    jmp exit

child:
    ; write(1, "child\n", len)
    mov rax, 1
    mov rdi, 1
    mov rsi, child_msg
    mov rdx, child_len
    int 128

exit:
    ; exit(0)
    mov rdi, rax       ; status = 0
    mov rax, 60        ; sys_exit
    int 128