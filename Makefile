PROJECT := UN_Orion
BUILD := build
ESP := $(BUILD)/esp

CC := clang
LD := ld.lld

KERNEL_CFLAGS := -target x86_64-unknown-none -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -Wall -Wextra -O2 -Iinclude -I$(BUILD)/generated
KERNEL_LDFLAGS := -nostdlib -static -T kernel/linker.ld
KERNEL_C_SRCS := kernel/main.c kernel/serial.c kernel/graphics.c kernel/interrupts.c kernel/pmm.c kernel/desktop.c
KERNEL_OBJS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(KERNEL_C_SRCS)) $(BUILD)/arch.o

EFIINC := /usr/include/efi
EFICRT := /usr/lib/crt0-efi-x86_64.o
EFILDS := /usr/lib/elf_x86_64_efi.lds
EFILIBDIR := /usr/lib
OVMF_CODE ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE.fd /usr/share/OVMF/OVMF_CODE_4M.fd))
OVMF_VARS ?= $(if $(findstring _4M,$(OVMF_CODE)),/usr/share/OVMF/OVMF_VARS_4M.fd,$(firstword $(wildcard /usr/share/OVMF/OVMF_VARS.fd /usr/share/OVMF/OVMF_VARS_4M.fd)))

.PHONY: all clean run image kernel smoke
all: image
kernel: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/generated/.fontstamp: tools/fontgen.py | $(BUILD)
	mkdir -p $(BUILD)/generated
	python3 tools/fontgen.py $(BUILD)/generated
	touch $@

$(BUILD)/%.o: kernel/%.c include/bootinfo.h $(BUILD)/generated/.fontstamp | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/arch.o: kernel/arch.S | $(BUILD)
	$(CC) -target x86_64-unknown-none -ffreestanding -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJS)
	$(LD) $(KERNEL_LDFLAGS) $^ -o $@

$(BUILD)/boot.o: boot/main.c include/bootinfo.h | $(BUILD)
	$(CC) -I$(EFIINC) -I$(EFIINC)/x86_64 -Iinclude -fpic -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -Wextra -c $< -o $@

$(BUILD)/BOOTX64.so: $(BUILD)/boot.o
	ld -nostdlib -znocombreloc -T $(EFILDS) -shared -Bsymbolic $(EFICRT) $< -L$(EFILIBDIR) -lefi -lgnuefi -o $@

$(BUILD)/BOOTX64.EFI: $(BUILD)/BOOTX64.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $< $@

image: $(BUILD)/kernel.elf $(BUILD)/BOOTX64.EFI tools/mkfat.py
	python3 tools/mkfat.py $(BUILD)/orion.img $(BUILD)/BOOTX64.EFI $(BUILD)/kernel.elf

$(BUILD)/OVMF_VARS.fd: | $(BUILD)
	cp $(OVMF_VARS) $@

run: image $(BUILD)/OVMF_VARS.fd
	qemu-system-x86_64 -machine q35 -m 512M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-nic none -serial stdio

smoke: image $(BUILD)/OVMF_VARS.fd
	rm -f $(BUILD)/serial.log
	@set +e; timeout 10s qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-nic none -display none -monitor none -serial file:$(BUILD)/serial.log; rc=$$?; \
		if [ $$rc -ne 0 ] && [ $$rc -ne 124 ]; then exit $$rc; fi
	grep -q "UN_Orion kernel 0.0.4 alive" $(BUILD)/serial.log
	grep -q "PMM ready" $(BUILD)/serial.log
	grep -q "IDT/PIC/PIT/keyboard ready" $(BUILD)/serial.log
	grep -q "Orion desktop ready" $(BUILD)/serial.log
	@echo "QEMU smoke test passed"

clean:
	rm -rf $(BUILD)
