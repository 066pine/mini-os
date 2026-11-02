; SharkX OS - x86_64 Multiboot2 entry
; GRUB loads this ELF at 1 MiB, we build identity page tables and enter long mode.

BITS 32

global _start
extern kernel_main
extern __bss_start
extern __bss_end

section .multiboot progbits alloc
align 8

mb2_header:
    dd 0xE85250D6
    dd 0
    dd mb2_header_end - mb2_header
    dd -(0xE85250D6 + (mb2_header_end - mb2_header))

    ; End tag
    dw 0
    dw 0
    dd 8

mb2_header_end:

section .bss
align 16

stack_bottom:
    resb 32768
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
_start:
    cli

    ; Preserve Multiboot2 magic (EAX) in ESI. EBX already holds the
    ; information pointer and is not touched by the setup below.
    mov esi, eax

    ; Zero .bss. This includes the boot stack and page tables.
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; Re-establish the stack after .bss was zeroed.
    mov esp, stack_top

    ; PML4[0] -> PDPT
    mov eax, pdpt
    or eax, 0x03
    mov [pml4], eax
    mov dword [pml4 + 4], 0

    ; PDPT[0] -> PD
    mov eax, pd
    or eax, 0x03
    mov [pdpt], eax
    mov dword [pdpt + 4], 0

    ; Identity map the first 1 GiB with 2 MiB pages.
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

    ; Enable PAE.
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Load PML4.
    mov eax, pml4
    mov cr3, eax

    ; Enable long mode in EFER.
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable paging.
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Load our GDT and enter 64-bit mode.
    lgdt [gdt64.pointer]
    jmp 0x08:long_mode_start

BITS 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    and rsp, -16
    xor rbp, rbp

    ; Multiboot2: RDI = magic, RSI = information pointer.
    mov edi, esi
    mov esi, ebx
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
