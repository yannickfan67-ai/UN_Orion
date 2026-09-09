PROJECT := UN_Orion
VERSION := 0.0.7
BUILD := build
ESP := $(BUILD)/esp
LEGACY := $(BUILD)/legacy-i686

CC := clang
LD := ld.lld

KERNEL_CFLAGS := -target x86_64-unknown-none -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -Wall -Wextra -O2 -Iinclude -I$(BUILD)/generated -DORION_ARCH_NAME=\"x86_64\"
KERNEL_LDFLAGS := -nostdlib -static -T kernel/linker.ld
KERNEL_C_SRCS := kernel/main.c kernel/serial.c kernel/graphics.c kernel/interrupts.c kernel/pmm.c kernel/desktop.c kernel/pci.c kernel/netdev.c kernel/netdev_rtl8139.c kernel/netdev_pcnet.c kernel/netdev_e1000.c kernel/netdev_virtio.c kernel/net.c kernel/net_resource.c kernel/aster.c kernel/vela.c kernel/vela_image.c kernel/browser_image.c
KERNEL_OBJS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(KERNEL_C_SRCS)) $(BUILD)/arch.o

I686_CFLAGS := -target i386-unknown-none -march=i686 -ffreestanding -fno-stack-protector -fno-pic -fno-builtin -mgeneral-regs-only -mno-sse -mno-sse2 -mno-mmx -Wall -Wextra -O2 -Iinclude -I$(BUILD)/generated -DORION_ARCH_NAME=\"i686\"
I686_LDFLAGS := -nostdlib -static -m elf_i386 -T kernel/i686/linker.ld
I686_C_SRCS := $(filter-out kernel/interrupts.c,$(KERNEL_C_SRCS)) kernel/i686/interrupts.c kernel/i686/builtins.c
I686_OBJS := $(patsubst kernel/%.c,$(LEGACY)/kernel/%.o,$(I686_C_SRCS)) $(LEGACY)/kernel/i686/entry.o $(LEGACY)/kernel/i686/arch.o

EFIINC := /usr/include/efi
EFICRT := /usr/lib/crt0-efi-x86_64.o
EFILDS := /usr/lib/elf_x86_64_efi.lds
EFILIBDIR := /usr/lib
OVMF_CODE ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd))
OVMF_VARS ?= $(if $(findstring _4M,$(OVMF_CODE)),/usr/share/OVMF/OVMF_VARS_4M.fd,$(firstword $(wildcard /usr/share/OVMF/OVMF_VARS.fd /usr/share/OVMF/OVMF_VARS_4M.fd)))

.PHONY: all clean run image iso kernel smoke iso-smoke install-smoke network-smoke legacy-i686 legacy-smoke aster-smoke
all: image
kernel: $(BUILD)/kernel.elf

aster-smoke: | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude kernel/aster.c tests/aster_selector_smoke.c -o $(BUILD)/aster-selector-smoke
	./$(BUILD)/aster-selector-smoke

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

image: $(BUILD)/kernel.elf $(BUILD)/BOOTX64.EFI tools/mkfat.py tools/image_manifest.py
	python3 tools/mkfat.py $(BUILD)/orion.img $(BUILD)/BOOTX64.EFI $(BUILD)/kernel.elf
	python3 tools/image_manifest.py --input $(BUILD)/orion.img --output $(BUILD)/orion-image.json --media disk-image --arch x86_64 --version $(VERSION)

iso: image tools/mkiso.py tools/image_manifest.py
	python3 tools/mkiso.py $(BUILD)/orion.img $(BUILD)/UN_Orion-v$(VERSION)-install.iso
	python3 tools/image_manifest.py --input $(BUILD)/UN_Orion-v$(VERSION)-install.iso --output $(BUILD)/orion-install.json --media installer-iso --arch x86_64 --version $(VERSION)

$(LEGACY):
	mkdir -p $(LEGACY)

$(LEGACY)/boot.o: compat/i686-bios/boot.S | $(LEGACY)
	$(CC) -target i386-unknown-none -ffreestanding -c $< -o $@

$(LEGACY)/boot.bin: $(LEGACY)/boot.o
	$(LD) -m elf_i386 -Ttext 0x7c00 --oformat binary $< -o $@

$(LEGACY)/stage2.o: compat/i686-bios/stage2.S | $(LEGACY)
	$(CC) -target i386-unknown-none -ffreestanding -c $< -o $@

$(LEGACY)/stage2.elf: $(LEGACY)/stage2.o compat/i686-bios/link.ld
	$(LD) -m elf_i386 -T compat/i686-bios/link.ld $< -o $@

$(LEGACY)/stage2.bin: $(LEGACY)/stage2.elf
	llvm-objcopy -O binary $< $@

$(LEGACY)/kernel/%.o: kernel/%.c include/bootinfo.h $(BUILD)/generated/.fontstamp | $(LEGACY)
	mkdir -p $(@D)
	$(CC) $(I686_CFLAGS) -c $< -o $@

$(LEGACY)/kernel/i686/entry.o: kernel/i686/entry.S | $(LEGACY)
	mkdir -p $(@D)
	$(CC) -target i386-unknown-none -march=i686 -ffreestanding -c $< -o $@

$(LEGACY)/kernel/i686/arch.o: kernel/i686/arch.S | $(LEGACY)
	mkdir -p $(@D)
	$(CC) -target i386-unknown-none -march=i686 -ffreestanding -c $< -o $@

$(LEGACY)/kernel32.elf: $(I686_OBJS) kernel/i686/linker.ld
	$(LD) $(I686_LDFLAGS) $(I686_OBJS) -o $@

$(LEGACY)/kernel32.bin: $(LEGACY)/kernel32.elf
	llvm-objcopy -O binary $< $@

legacy-i686: $(BUILD)/UN_Orion-v$(VERSION)-i686-bios.img

$(BUILD)/UN_Orion-v$(VERSION)-i686-bios.img: $(LEGACY)/boot.bin $(LEGACY)/stage2.bin $(LEGACY)/kernel32.bin tools/mklegacy.py tools/image_manifest.py
	python3 tools/mklegacy.py $(LEGACY)/boot.bin $(LEGACY)/stage2.bin $@ $(LEGACY)/kernel32.bin
	python3 tools/image_manifest.py --input $@ --output $(BUILD)/orion-i686-bios.json --media disk-image --arch i686 --version $(VERSION)

legacy-smoke: $(BUILD)/UN_Orion-v$(VERSION)-i686-bios.img
	rm -f $(BUILD)/serial-i686.log
	@set +e; timeout 15s qemu-system-i386 -machine pc -m 128M \
		-drive if=floppy,format=raw,file=$(BUILD)/UN_Orion-v$(VERSION)-i686-bios.img -boot a \
		-netdev user,id=n0 -device rtl8139,netdev=n0,romfile= \
		-display none -monitor none -serial file:$(BUILD)/serial-i686.log; rc=$$?; \
		if [ $$rc -ne 0 ] && [ $$rc -ne 124 ]; then exit $$rc; fi
	grep -q "UN_Orion $(VERSION) alive" $(BUILD)/serial-i686.log
	grep -q "PMM ready" $(BUILD)/serial-i686.log
	grep -q "IDT/PIC/PIT/keyboard ready" $(BUILD)/serial-i686.log
	grep -q "Orion desktop ready" $(BUILD)/serial-i686.log
	grep -q "Network stack ready" $(BUILD)/serial-i686.log
	@echo "i686 Legacy BIOS full-system smoke test passed"

$(BUILD)/OVMF_VARS.fd: | $(BUILD)
	cp $(OVMF_VARS) $@

run: image $(BUILD)/OVMF_VARS.fd
	qemu-system-x86_64 -machine q35 -m 512M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-netdev user,id=n0 -device rtl8139,netdev=n0,romfile= -serial stdio

smoke: image
	cp $(OVMF_VARS) $(BUILD)/OVMF_VARS-smoke.fd
	rm -f $(BUILD)/serial.log
	@set +e; timeout 10s qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS-smoke.fd \
		-drive format=raw,file=$(BUILD)/orion.img \
		-netdev user,id=n0 -device rtl8139,netdev=n0,romfile= -display none -monitor none -serial file:$(BUILD)/serial.log; rc=$$?; \
		if [ $$rc -ne 0 ] && [ $$rc -ne 124 ]; then exit $$rc; fi
	grep -q "UN_Orion $(VERSION) alive" $(BUILD)/serial.log
	grep -q "PMM ready" $(BUILD)/serial.log
	grep -q "IDT/PIC/PIT/keyboard ready" $(BUILD)/serial.log
	grep -q "Orion desktop ready" $(BUILD)/serial.log
	grep -q "Network stack ready" $(BUILD)/serial.log
	@echo "QEMU smoke test passed"

iso-smoke: iso
	cp $(OVMF_VARS) $(BUILD)/OVMF_VARS-iso.fd
	rm -f $(BUILD)/serial-iso.log
	@set +e; timeout 12s qemu-system-x86_64 -machine q35 -m 256M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS-iso.fd \
		-cdrom $(BUILD)/UN_Orion-v$(VERSION)-install.iso -boot d \
		-netdev user,id=n0 -device rtl8139,netdev=n0,romfile= -display none -monitor none -serial file:$(BUILD)/serial-iso.log; rc=$$?; \
		if [ $$rc -ne 0 ] && [ $$rc -ne 124 ]; then exit $$rc; fi
	grep -q "UN_Orion bootloader" $(BUILD)/serial-iso.log
	grep -q "UN_Orion $(VERSION) alive" $(BUILD)/serial-iso.log
	grep -q "Orion desktop ready" $(BUILD)/serial-iso.log
	@echo "UEFI install ISO smoke test passed"

install-smoke: iso
	python3 scripts/install_smoke.py

network-smoke: image
	python3 scripts/network_smoke.py

clean:
	rm -rf $(BUILD)
