section .data
    parent_msg db "parent", 10
    parent_len equ $ - parent_msg

    child_msg db "child", 10
    child_len equ $ - child_msg

    done_msg db "child exited with status: ", 10
    done_len equ $ - done_msg

    start_fork db "fork start: ", 10
    start_fork_len equ $ - start_fork

section .bss
    status resq 1

section .text
global _start

_start:
    ; print "child exited..."
    mov rax, 1
    mov rdi, 1
    mov rsi, start_fork
    mov rdx, start_fork_len
    int 128

    ; fork()
    mov rax, 57
    int 128

    mov r9, rax

    mov rax, 255
    mov rdi, r9
    int 128

    mov rax, r9

    cmp rax, 0
    je child
    jmp parent

; ======================
; PARENT
; ======================
parent:
    ; rax = child PID
    mov rdi, rax        ; pid
    mov rsi, status     ; int *status
    mov rax, 61         ; sys_waitpid
    int 128

    ; print "parent"
    mov rax, 1
    mov rdi, 1
    mov rsi, parent_msg
    mov rdx, parent_len
    int 128

    ; print "child exited..."
    mov rax, 1
    mov rdi, 1
    mov rsi, done_msg
    mov rdx, done_len
    int 128

    ; print exit code (very simple: only 1 digit)
    mov rax, [status]
    add rax, '0'

    mov [status], rax

    mov rax, 1
    mov rdi, 1
    mov rsi, status
    mov rdx, 1
    int 128

    jmp exit

; ======================
; CHILD
; ======================
child:
    ; print "child"
    mov rax, 1
    mov rdi, 1
    mov rsi, child_msg
    mov rdx, child_len
    int 128

    ; exit(5)
    mov rdi, 5
    mov rax, 60
    int 128

; ======================
exit:
    mov rdi, 0
    mov rax, 60
    int 128