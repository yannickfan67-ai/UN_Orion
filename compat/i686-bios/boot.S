.code16
.global _start
_start:
    cli
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movw $0x7c00,%sp
    sti
    movb %dl,boot_drive

    # Load 16 sectors (CHS 0/0/2..17) to 0000:8000.
    movw $0x8000,%bx
    movb $0x02,%ah
    movb $16,%al
    xorb %ch,%ch
    movb $2,%cl
    xorb %dh,%dh
    movb boot_drive,%dl
    int $0x13
    jc fail
    ljmp $0x0000,$0x8000

fail:
    movw $0xb800,%ax
    movw %ax,%es
    movw $0x4f45,%es:0
1:
    hlt
    jmp 1b

boot_drive:
    .byte 0
.org 510
.word 0xaa55
