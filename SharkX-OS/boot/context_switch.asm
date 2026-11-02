; SharkX OS - cooperative context switch and new-thread trampoline

BITS 64

global context_switch
global thread_trampoline

extern thread_exit_c

section .text

; void context_switch(uint64_t* old_rsp, uint64_t new_rsp)
; RDI = address where the current RSP is saved
; RSI = RSP of the next thread
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp
    mov rsp, rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; New threads are entered here by context_switch's final RET.
; R12 holds the entry function, R13 holds the argument.
thread_trampoline:
    sti
    mov rdi, r13
    call r12
    call thread_exit_c

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
