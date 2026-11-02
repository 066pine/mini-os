
; SharkX OS - x86_64 boot entry
; GRUB Multiboot2 -> 64-bit long mode

BITS 32

section .multiboot progbits alloc
align 8

mb2_header:
    dd 0xE85250D6
    dd 0
    dd mb2_header_end - mb2_header
    dd -(0xE85250D6 + (mb2_header_end - mb2_header))

    dw 0
    dw 0
    dd 8

mb2_header_end:

section .bss
align 16

stack_bottom:
    resb 16384
stack_top:

align 4096
pml4:
    resq 512

align 4096
pdpt:
    resq 512

align 4096
pd:
    resq 512

section .text
global _start
extern kernel_main

_start:
    cli

    ; Setup stack
    mov esp, stack_top

    ; PML4 -> PDPT
    mov eax, pdpt
    or eax, 0x03
    mov [pml4], eax

    ; PDPT -> PD
    mov eax, pd
    or eax, 0x03
    mov [pdpt], eax

    ; Map first 1 GiB using 2 MiB pages
    mov ecx, 0

.map_pages:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83

    mov [pd + ecx * 8], eax
    mov dword [pd + ecx * 8 + 4], 0

    inc ecx
    cmp ecx, 512
    jne .map_pages

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load page table
    mov eax, pml4
    mov cr3, eax

    ; Enable Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [gdt64.pointer]

    ; Far jump into 64-bit mode
    jmp 0x08:long_mode_start

BITS 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rsp, stack_top
    xor rbp, rbp

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF

.pointer:
    dw $ - gdt64 - 1
    dq gdt64
