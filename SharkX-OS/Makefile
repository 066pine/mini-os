
TARGET = x86_64-elf

CC = $(TARGET)-g++
LD = $(TARGET)-ld
AS = nasm

BUILD = build
ISO_DIR = iso

CFLAGS = -std=c++17 \
         -ffreestanding \
         -O2 \
         -Wall \
         -Wextra \
         -fno-exceptions \
         -fno-rtti \
         -fno-stack-protector \
         -fno-pic \
         -fno-pie \
         -m64 \
         -mcmodel=small \
         -mno-red-zone \
         -mno-mmx \
         -mno-sse \
         -mno-sse2 \
         -msoft-float

LDFLAGS = -T linker.ld \
          -nostdlib \
          -z max-page-size=0x1000

all: iso

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: boot/entry.asm | $(BUILD)
	$(AS) -f elf64 $< -o $@

$(BUILD)/main.o: kernel/main.cpp | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/main.o
	$(LD) $(LDFLAGS) $^ -o $@

iso: $(BUILD)/kernel.elf
	mkdir -p $(ISO_DIR)/boot
	cp $(BUILD)/kernel.elf $(ISO_DIR)/boot/kernel.elf
	grub-mkrescue -o $(BUILD)/SharkX.iso $(ISO_DIR)

run: iso
	qemu-system-x86_64 \
		-cdrom $(BUILD)/SharkX.iso \
		-m 512M \
		-serial stdio \
		-display none \
		-no-reboot \
		-no-shutdown

clean:
	rm -rf $(BUILD)/*
	rm -f $(ISO_DIR)/boot/kernel.elf

.PHONY: all iso run clean
