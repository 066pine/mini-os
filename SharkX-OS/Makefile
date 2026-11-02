CXX := g++
LD := x86_64-elf-ld
AS := nasm
QEMU := qemu-system-x86_64

BUILD := build
ISO_DIR := iso
KERNEL := $(BUILD)/kernel.elf
ISO := $(BUILD)/SharkX.iso

GCC_INCLUDE := $(shell $(CXX) -print-file-name=include)

CXXFLAGS := -std=c++17 \
            -ffreestanding \
            -O2 \
            -Wall \
            -Wextra \
            -Wno-unused-parameter \
            -fno-exceptions \
            -fno-rtti \
            -fno-stack-protector \
            -fno-pic \
            -fno-pie \
            -fno-builtin \
            -fno-threadsafe-statics \
            -fno-use-cxa-atexit \
            -fno-asynchronous-unwind-tables \
            -fno-unwind-tables \
            -m64 \
            -mcmodel=small \
            -mno-red-zone \
            -mno-mmx \
            -mno-sse \
            -mno-sse2 \
            -msoft-float \
            -nostdinc \
            -isystem $(GCC_INCLUDE) \
            -Iinclude \
            -MMD -MP

ASFLAGS := -f elf64

LDFLAGS := -T linker.ld \
           -nostdlib \
           -z max-page-size=0x1000 \
           -z noexecstack \
           --build-id=none

CXX_SOURCES := $(shell find kernel -name '*.cpp' | sort)
ASM_SOURCES := $(wildcard boot/*.asm)

OBJECTS := $(patsubst %.cpp,$(BUILD)/%.o,$(CXX_SOURCES))
OBJECTS += $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SOURCES))

all: iso

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	grub-mkrescue -o $(ISO) $(ISO_DIR) >/dev/null
	@echo "Built $(ISO)"

run: iso
	$(QEMU) -cdrom $(ISO) -m 512M -serial stdio -monitor none -display none \
		-no-reboot -device isa-debug-exit,iobase=0x501,iosize=0x04

debug: iso
	$(QEMU) -cdrom $(ISO) -m 512M -serial stdio -monitor none -display none \
		-no-reboot -s -S

test: iso
	./tests/run-qemu-tests.sh

check-toolchain:
	@$(CXX) --version | head -n 1
	@$(LD) --version | head -n 1
	@$(AS) -v 2>&1 | head -n 1
	@grub-file --version 2>&1 | head -n 1
	@$(QEMU) --version | head -n 1

clean:
	rm -rf $(BUILD)/*
	rm -f $(ISO_DIR)/boot/kernel.elf

.PHONY: all iso run debug test check-toolchain clean

-include $(OBJECTS:.o=.d)
