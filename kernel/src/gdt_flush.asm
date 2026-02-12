global gdt_reload
gdt_reload:
    lgdt [rdi]
    push 0x28
    lea rax, [rel .reload_CS]
    push rax
    retfq
.reload_CS:
    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global tss_load
tss_load:
    mov ax, 0x38
    ltr ax
    ret