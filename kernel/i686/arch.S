.text
.code32
.global orion_load_gdt
.type orion_load_gdt,@function
orion_load_gdt:
    movl 4(%esp), %eax
    lgdt (%eax)
    ljmp $0x08, $1f
1:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    ret

.macro ISR_NOERR n
.global orion_i686_isr\n
.type orion_i686_isr\n,@function
orion_i686_isr\n:
    pushl $0
    pushl $\n
    jmp orion_i686_exception_common
.endm

.macro ISR_ERR n
.global orion_i686_isr\n
.type orion_i686_isr\n,@function
orion_i686_isr\n:
    pushl $\n
    jmp orion_i686_exception_common
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

.extern orion_exception_panic32
.type orion_i686_exception_common,@function
orion_i686_exception_common:
    cld
    movl %esp,%eax
    pushl 16(%eax)     # eflags
    pushl 12(%eax)     # cs
    pushl 8(%eax)      # eip
    pushl 4(%eax)      # error
    pushl 0(%eax)      # vector
    call orion_exception_panic32
2:
    cli
    hlt
    jmp 2b

.section .rodata
.align 4
.global orion_exception_stub_table
orion_exception_stub_table:
    .long orion_i686_isr0,orion_i686_isr1,orion_i686_isr2,orion_i686_isr3
    .long orion_i686_isr4,orion_i686_isr5,orion_i686_isr6,orion_i686_isr7
    .long orion_i686_isr8,orion_i686_isr9,orion_i686_isr10,orion_i686_isr11
    .long orion_i686_isr12,orion_i686_isr13,orion_i686_isr14,orion_i686_isr15
    .long orion_i686_isr16,orion_i686_isr17,orion_i686_isr18,orion_i686_isr19
    .long orion_i686_isr20,orion_i686_isr21,orion_i686_isr22,orion_i686_isr23
    .long orion_i686_isr24,orion_i686_isr25,orion_i686_isr26,orion_i686_isr27
    .long orion_i686_isr28,orion_i686_isr29,orion_i686_isr30,orion_i686_isr31
.section .note.GNU-stack,"",@progbits
