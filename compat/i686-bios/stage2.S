.code16
.global _start
.set HDR_BUF, 0x4000
.set VBE_BUF, 0x5000
.set MEMDESC_BUF, 0x6000
.set BOOTINFO_BUF, 0x7000
.set KERNEL_TMP_SEG, 0x1000
.set KERNEL_LBA, 18

_start:
    cli
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movw $0x7b00,%sp
    sti
    movb %dl,boot_drive

    # Read the one-sector Orion i686 kernel header (LBA 17).
    movw $17,current_lba
    movw $0x0400,%ax
    movw %ax,%es
    xorw %bx,%bx
    call read_one
    jc disk_fail
    cmpw $0x524f,%es:0       # "OR" from ORK2
    jne header_fail
    cmpw $0x324b,%es:2       # "K2"
    jne header_fail

    # Load the raw i686 kernel to 0x10000, one floppy sector at a time.
    movw %es:8,%cx           # sector count
    testw %cx,%cx
    jz header_fail
    movw $KERNEL_LBA,current_lba
    movw $KERNEL_TMP_SEG,%ax
    movw %ax,%es
    xorw %bx,%bx
1:
    pushw %cx
    call read_one
    jc disk_fail_pop
    movw %es,%ax
    addw $0x20,%ax
    movw %ax,%es
    incw current_lba
    popw %cx
    loop 1b

    # Obtain a linear 32-bit framebuffer. SeaVGABIOS/Bochs VBE exposes
    # 32-bpp modes in the 0x14x range; real VBE BIOSes may use 0x11x.
    movw $0x144,%cx
    call try_vbe
    jnc vbe_ok
    movw $0x143,%cx
    call try_vbe
    jnc vbe_ok
    movw $0x142,%cx
    call try_vbe
    jnc vbe_ok
    movw $0x118,%cx
    call try_vbe
    jnc vbe_ok
    movw $0x115,%cx
    call try_vbe
    jnc vbe_ok
    jmp vbe_fail
vbe_ok:

    # Build one EFI-compatible conventional-memory descriptor for the common PMM.
    xorw %ax,%ax
    movw %ax,%es
    movl $0,MEMDESC_BUF+0
    movl $0,MEMDESC_BUF+4
    movl $7,MEMDESC_BUF+0
    movl $0x01000000,MEMDESC_BUF+8
    movl $0,MEMDESC_BUF+12
    movl $0,MEMDESC_BUF+16
    movl $0,MEMDESC_BUF+20
    movl $0,MEMDESC_BUF+24
    movl $0,MEMDESC_BUF+28
    movl $0,MEMDESC_BUF+32
    movl $0,MEMDESC_BUF+36

    # E801 reports RAM above 16 MiB in 64 KiB blocks (BX/DX depending BIOS).
    movw $0xe801,%ax
    int $0x15
    jc mem_default
    testw %bx,%bx
    jnz mem_bx
    testw %dx,%dx
    jnz mem_dx
    movzwl %ax,%eax
    shll $10,%eax
    addl $0x00100000,%eax
    cmpl $0x01000000,%eax
    ja mem_top_ready
    movl $0x04000000,%eax
    jmp mem_top_ready
mem_bx:
    movzwl %bx,%eax
    shll $16,%eax
    addl $0x01000000,%eax
    jmp mem_top_ready
mem_dx:
    movzwl %dx,%eax
    shll $16,%eax
    addl $0x01000000,%eax
    jmp mem_top_ready
mem_default:
    movl $0x04000000,%eax
mem_top_ready:
    subl $0x01000000,%eax
    shrl $12,%eax
    movl %eax,MEMDESC_BUF+24

    # OrionBootInfo at 0x7000. VBE mode info remains at 0x5000.
    movl VBE_BUF+40,%eax
    movl %eax,BOOTINFO_BUF+0
    movl $0,BOOTINFO_BUF+4
    movzwl VBE_BUF+50,%eax
    testl %eax,%eax
    jnz pitch_ready
    movzwl VBE_BUF+16,%eax
pitch_ready:
    movzwl VBE_BUF+20,%ecx
    imull %ecx,%eax
    movl %eax,BOOTINFO_BUF+8
    movl $0,BOOTINFO_BUF+12
    movzwl VBE_BUF+18,%eax
    movl %eax,BOOTINFO_BUF+16
    movzwl VBE_BUF+20,%eax
    movl %eax,BOOTINFO_BUF+20
    movzwl VBE_BUF+50,%eax
    testl %eax,%eax
    jnz pitch2_ready
    movzwl VBE_BUF+16,%eax
pitch2_ready:
    shrl $2,%eax
    movl %eax,BOOTINFO_BUF+24
    movl $1,BOOTINFO_BUF+28
    movl $MEMDESC_BUF,BOOTINFO_BUF+32
    movl $0,BOOTINFO_BUF+36
    movl $40,BOOTINFO_BUF+40
    movl $0,BOOTINFO_BUF+44
    movl $40,BOOTINFO_BUF+48
    movl $0,BOOTINFO_BUF+52
    movl $1,BOOTINFO_BUF+56
    movl $0,BOOTINFO_BUF+60

    cli
    inb $0x92,%al
    orb $0x02,%al
    andb $0xfe,%al
    outb %al,$0x92
    lgdt gdt_desc
    movl %cr0,%eax
    orl $1,%eax
    movl %eax,%cr0
    ljmpl $0x08,$pm_entry

# CX = VBE mode. CF clear on success.
try_vbe:
    pushw %cx
    xorw %ax,%ax
    movw %ax,%es
    movw $VBE_BUF,%di
    movw $0x4f01,%ax
    int $0x10
    cmpw $0x004f,%ax
    jne 1f
    testw $0x0080,VBE_BUF+0
    jz 1f
    cmpb $32,VBE_BUF+25
    jne 1f
    popw %cx
    movw %cx,%bx
    orw $0x4000,%bx
    movw $0x4f02,%ax
    int $0x10
    cmpw $0x004f,%ax
    jne 2f
    clc
    ret
1:
    popw %cx
2:
    stc
    ret

# Read current_lba into ES:BX using 1.44 MiB geometry (18 spt, 2 heads).
read_one:
    pusha
    movw current_lba,%ax
    xorw %dx,%dx
    movw $36,%si
    divw %si
    movb %al,%ch
    movw %dx,%ax
    xorw %dx,%dx
    movw $18,%si
    divw %si
    movb %al,%dh
    movb %dl,%cl
    incb %cl
    movb boot_drive,%dl
    movb $0x02,%ah
    movb $1,%al
    int $0x13
    popa
    ret

disk_fail_pop:
    popw %cx
disk_fail:
    movw $msg_disk,%si
    jmp bios_error
header_fail:
    movw $msg_hdr,%si
    jmp bios_error
vbe_fail:
    movw $msg_vbe,%si
bios_error:
    movb $0x0e,%ah
1:
    lodsb
    testb %al,%al
    jz 2f
    int $0x10
    jmp 1b
2:
    cli
3:
    hlt
    jmp 3b

.code32
pm_entry:
    movw $0x10,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movw %ax,%fs
    movw %ax,%gs
    movl $0x0009f000,%esp
    cld

    # Copy raw kernel file from low-memory staging area to its linked 1 MiB address.
    movl $HDR_BUF,%esi
    movl 4(%esi),%ecx
    movl $0x00010000,%esi
    movl $0x00100000,%edi
    rep movsb

    movl $BOOTINFO_BUF,%ebx
    movl $0x00100000,%eax
    jmp *%eax

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_desc:
    .word gdt_end-gdt-1
    .long gdt

boot_drive: .byte 0
current_lba: .word 0
msg_disk: .asciz "UN_Orion i686: disk read failed\r\n"
msg_hdr:  .asciz "UN_Orion i686: kernel header invalid\r\n"
msg_vbe:  .asciz "UN_Orion i686: VBE 32-bit framebuffer unavailable\r\n"
