PROJECT := UN_Orion
BUILD := build
ESP := $(BUILD)/esp

CC := clang
LD := ld.lld

KERNEL_CFLAGS := -target x86_64-unknown-none -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -Wall -Wextra -O2 -Iinclude
KERNEL_LDFLAGS := -nostdlib -static -T kernel/linker.ld

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

$(BUILD)/kernel.o: kernel/main.c include/bootinfo.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/kernel.o
	$(LD) $(KERNEL_LDFLAGS) $< -o $@

$(BUILD)/boot.o: boot/main.c include/bootinfo.h | $(BUILD)
	$(CC) -I$(EFIINC) -I$(EFIINC)/x86_64 -Iinclude -fpic -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -Wextra -c $< -o $@

$(BUILD)/BOOTX64.so: $(BUILD)/boot.o
	ld -nostdlib -znocombreloc -T $(EFILDS) -shared -Bsymbolic $(EFICRT) $< -L$(EFILIBDIR) -lefi -lgnuefi -o $@

$(BUILD)/BOOTX64.EFI: $(BUILD)/BOOTX64.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $< $@

image: $(BUILD)/kernel.elf $(BUILD)/BOOTX64.EFI
	rm -rf $(ESP)
	mkdir -p $(ESP)/EFI/BOOT
	cp $(BUILD)/BOOTX64.EFI $(ESP)/EFI/BOOT/BOOTX64.EFI
	cp $(BUILD)/kernel.elf $(ESP)/kernel.elf
	dd if=/dev/zero of=$(BUILD)/orion.img bs=1M count=64 status=none
	mkfs.vfat $(BUILD)/orion.img >/dev/null
	mmd -i $(BUILD)/orion.img ::/EFI ::/EFI/BOOT
	mcopy -i $(BUILD)/orion.img $(ESP)/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(BUILD)/orion.img $(ESP)/kernel.elf ::/kernel.elf
	@echo "Built $(BUILD)/orion.img"

$(BUILD)/OVMF_VARS.fd: | $(BUILD)
	cp $(OVMF_VARS) $@

run: image $(BUILD)/OVMF_VARS.fd
	qemu-system-x86_64 -machine q35 -m 512M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-serial stdio

smoke: image $(BUILD)/OVMF_VARS.fd
	rm -f $(BUILD)/serial.log
	@set +e; timeout 10s qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-display none -monitor none -serial file:$(BUILD)/serial.log; rc=$$?; \
		if [ $$rc -ne 0 ] && [ $$rc -ne 124 ]; then exit $$rc; fi
	grep -q "UN_Orion kernel alive" $(BUILD)/serial.log
	@echo "QEMU smoke test passed"

clean:
	rm -rf $(BUILD)
