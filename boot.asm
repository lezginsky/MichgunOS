[bits 32]

mbalign     equ  1
meminfo     equ  2
vidinfo     equ  4
mbflags     equ  0x00000007
magic       equ  0x1badb002
checksum    equ  0xe4524ff7

extern __multiboot_start
extern _code_end
extern _data_end
extern _bss_end

section .multiboot
align 4
__multiboot_start:
    dd magic
    dd mbflags
    dd checksum
    dd __multiboot_start
    dd _start
    dd _data_end
    dd _bss_end
    dd _start
    dd 0
    dd 1024
    dd 768
    dd 32

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
_start:
    cli
    lgdt [gdt_desc]
    jmp 0x08:.reload_segments

.reload_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, stack_top

    push ebx
    extern idt_init
    extern kernel_main
    call idt_init
    sti
    pop ebx
    push ebx
    call kernel_main

.halt_loop:
    hlt
    jmp .halt_loop

global idt_load
idt_load:
    extern idtp
    lidt [idtp]
    ret

global timer_handler
timer_handler:
    pusha
    mov al, 0x20
    out 0x20, al
    popa
    iretd

global keyboard_handler
keyboard_handler:
    pusha
    xor eax, eax
    in al, 0x60
    push eax
    extern keyboard_callback
    call keyboard_callback
    add esp, 4
    mov al, 0x20
    out 0x20, al
    popa
    iretd

global mouse_handler
mouse_handler:
    pusha
    xor eax, eax
    in al, 0x60
    push eax
    extern mouse_callback
    call mouse_callback
    add esp, 4
    mov al, 0x20
    out 0xa0, al
    out 0x20, al
    popa
    iretd

global outb
outb:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, al
    ret

global inb
inb:
    mov edx, [esp + 4]
    xor eax, eax
    in al, dx
    ret

section .data
align 4
gdt_start:
    dd 0x0, 0x0             
gdt_code:
    dw 0xffff, 0x0          
    db 0x0, 0x9a, 0xcf, 0x0 
gdt_data:
    dw 0xffff, 0x0          
    db 0x0, 0x92, 0xcf, 0x0 
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .note.GNU-stack noalloc noexec nowrite progbits