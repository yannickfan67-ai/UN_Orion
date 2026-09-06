.text
.global orion_load_gdt
.type orion_load_gdt,@function
orion_load_gdt:
    lgdt (%rdi)
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    ret

.macro ISR_NOERR n
.global orion_isr\n
.type orion_isr\n,@function
orion_isr\n:
    pushq $0
    pushq $\n
    jmp orion_exception_common
.endm

.macro ISR_ERR n
.global orion_isr\n
.type orion_isr\n,@function
orion_isr\n:
    pushq $\n
    jmp orion_exception_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

.type orion_exception_common,@function
orion_exception_common:
    cld
    movq 0(%rsp), %rdi
    movq 8(%rsp), %rsi
    movq 16(%rsp), %rdx
    movq 24(%rsp), %rcx
    movq 32(%rsp), %r8
    andq $-16, %rsp
    call orion_exception_panic
    cli
2:  hlt
    jmp 2b

.section .rodata
.align 8
.global orion_exception_stub_table
orion_exception_stub_table:
    .quad orion_isr0,orion_isr1,orion_isr2,orion_isr3
    .quad orion_isr4,orion_isr5,orion_isr6,orion_isr7
    .quad orion_isr8,orion_isr9,orion_isr10,orion_isr11
    .quad orion_isr12,orion_isr13,orion_isr14,orion_isr15
    .quad orion_isr16,orion_isr17,orion_isr18,orion_isr19
    .quad orion_isr20,orion_isr21,orion_isr22,orion_isr23
    .quad orion_isr24,orion_isr25,orion_isr26,orion_isr27
    .quad orion_isr28,orion_isr29,orion_isr30,orion_isr31
.section .note.GNU-stack,"",@progbits
