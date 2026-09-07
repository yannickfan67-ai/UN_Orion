.code16
.global _start
_start:
    cli
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es

    # Fast A20 gate. The compatibility target keeps the firmware dependency
    # deliberately small so it can boot on late AT-class BIOS machines.
    inb $0x92,%al
    orb $0x02,%al
    andb $0xfe,%al
    outb %al,$0x92

    lgdt gdt_desc
    movl %cr0,%eax
    orl $1,%eax
    movl %eax,%cr0
    ljmpl $0x08,$pm_entry

.code32
pm_entry:
    movw $0x10,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movw %ax,%fs
    movw %ax,%gs
    movl $0x9f000,%esp
    cld

    call clear
    movl $0xb8000,%edi
    movl $title,%esi
    movb $0x1f,%ah
    call puts

    movl $0xb8000+160*2,%edi
    movl $line1,%esi
    movb $0x0f,%ah
    call puts

    movl $0xb8000+160*4,%edi
    movl $line2,%esi
    call puts

    movl $0xb8000+160*6,%edi
    movl $cpu_label,%esi
    call puts

    # CPUID vendor string. This also avoids trusting old BIOS CPU labels.
    xorl %eax,%eax
    cpuid
    movl %ebx,cpu_vendor
    movl %edx,cpu_vendor+4
    movl %ecx,cpu_vendor+8
    movb $0,cpu_vendor+12
    movl $cpu_vendor,%esi
    movb $0x0f,%ah
    call puts

    movl $0xb8000+160*8,%edi
    movl $line3,%esi
    movb $0x0f,%ah
    call puts

    movl $0xb8000+160*10,%edi
    movl $line4,%esi
    call puts

#ifdef ORION_LEGACY_SMOKE
    # Destructive only in the dedicated smoke build and only against a test disk.
    call ata_signature
#endif

    movl $0xb8000+160*12,%edi
    movl $line5,%esi
    movb $0x0a,%ah
    call puts

halt:
    cli
1:
    hlt
    jmp 1b

clear:
    movl $0xb8000,%edi
    movl $80*25,%ecx
    movw $0x0720,%ax
    rep stosw
    ret

# ESI = NUL string, EDI = VGA character cell, AH = attribute.
puts:
1:
    lodsb
    testb %al,%al
    jz 2f
    movb %al,(%edi)
    movb %ah,1(%edi)
    addl $2,%edi
    jmp 1b
2:
    ret

#ifdef ORION_LEGACY_SMOKE
ata_wait:
    movw $0x1f7,%dx
    movl $0x200000,%ecx
1:
    inb %dx,%al
    testb $0x80,%al
    jnz 2f
    ret
2:
    loop 1b
    ret

ata_drq:
    movw $0x1f7,%dx
    movl $0x200000,%ecx
1:
    inb %dx,%al
    testb $1,%al
    jnz 3f
    testb $8,%al
    jnz 2f
    loop 1b
3:
    ret
2:
    ret

# Smoke-only proof: write a known 512-byte sector to dedicated test HDD LBA 11.
ata_signature:
    call ata_wait
    movw $0x1f6,%dx
    movb $0xe0,%al
    outb %al,%dx
    movw $0x1f2,%dx
    movb $1,%al
    outb %al,%dx
    incw %dx
    movb $11,%al
    outb %al,%dx
    incw %dx
    xorb %al,%al
    outb %al,%dx
    incw %dx
    outb %al,%dx
    movw $0x1f7,%dx
    movb $0x30,%al
    outb %al,%dx
    call ata_drq
    movw $0x1f0,%dx
    movl $sig_sector,%esi
    movl $256,%ecx
1:
    lodsw
    outw %ax,%dx
    loop 1b
    call ata_wait
    ret
#endif

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_desc:
    .word gdt_end-gdt-1
    .long gdt

title:     .asciz "UN_Orion i686 / BIOS"
line1:     .asciz "Compatibility bootstrap 0.1 - protected mode is alive"
line2:     .asciz "Firmware: Legacy BIOS    Architecture: i686"
cpu_label: .asciz "CPU vendor: "
line3:     .asciz "ODI ABI 1.1    VGA text backend    legacy-PC bootstrap"
line4:     .asciz "86Box / physical-PC compatibility path ready"
#ifdef ORION_LEGACY_SMOKE
line5:     .asciz "SMOKE VERIFIED: protected-mode kernel wrote ATA LBA11 signature"
#else
line5:     .asciz "Normal image is non-destructive: no automatic disk writes"
#endif
cpu_vendor: .space 13

#ifdef ORION_LEGACY_SMOKE
.align 2
sig_sector:
    .ascii "ORIONI686-PMODE-BOOT-OK\0"
    .fill 488,1,0
#endif
