CC      := gcc
CXX     := g++
AS      := gcc
LD      := ld
OBJCOPY := objcopy

CFLAGS  := -ffreestanding -nostdlib -nostartfiles \
           -nodefaultlibs -m32 -mno-sse -mno-mmx -mno-80387 \
           -fno-stack-protector -fno-pic -fno-pie \
           -Wall -Wextra -Werror -O2 -g \
           -I. -I./hyperkernel -I./libc/include
CXXFLAGS:= $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS := -ffreestanding -nostdlib -m32 -g -c
LDFLAGS := -m elf_i386 -T arch/x86_64/linker.ld -nostdlib

HYPERKERNEL_OBJS := \
    arch/x86_64/boot.o \
    arch/x86_64/memory.o \
    arch/x86_64/interrupt.o \
    arch/x86_64/interrupt_wrapper.o \
    arch/x86_64/paging.o \
    arch/x86_64/syscalls.o \
    arch/x86_64/hypervisor.o \
    hyperkernel/hyperkernel.o \
    hyperkernel/instance_manager.o \
    hyperkernel/kernel_loader.o \
    hyperkernel/scheduler.o \
    hyperkernel/heartbeat.o \
    hyperkernel/logs.o \
    hyperkernel/crypto.o

HYPERKERNEL_ELF := hyperkernel.elf
HYPERKERNEL_BIN := hyperkernel.bin

BASE_KERNEL_OBJS := \
    arch/x86_64/boot_base.o \
    kernels/kernel_template.o

BASE_KERNEL_ELF := base_kernel.elf
BASE_KERNEL_BIN := kernels/base_kernel.img

BASE_KERNEL_EMBED := kernels/base_kernel_embed.o

.PHONY: all clean hyperkernel base_kernel check check-multiboot check-size qemu iso

all: hyperkernel base_kernel check

# ─── Embedded Base Kernel (binary → object) ────────────────────

$(BASE_KERNEL_EMBED): $(BASE_KERNEL_BIN)
	$(OBJCOPY) --input-target binary --output-target elf32-i386 \
	  --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	  $< $@

# ─── Hyperkernel ───────────────────────────────────────────────

hyperkernel: $(HYPERKERNEL_ELF) $(HYPERKERNEL_BIN)

$(HYPERKERNEL_ELF): $(HYPERKERNEL_OBJS) $(BASE_KERNEL_EMBED) arch/x86_64/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(HYPERKERNEL_OBJS) $(BASE_KERNEL_EMBED)

$(HYPERKERNEL_BIN): $(HYPERKERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

arch/x86_64/%.o: arch/x86_64/%.S
	$(AS) $(ASFLAGS) -o $@ $<

arch/x86_64/%.o: arch/x86_64/%.c hyperkernel/hyperkernel.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

hyperkernel/%.o: hyperkernel/%.c hyperkernel/hyperkernel.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

# ─── Base Kernel ───────────────────────────────────────────────

base_kernel: $(BASE_KERNEL_ELF) $(BASE_KERNEL_BIN)

$(BASE_KERNEL_ELF): $(BASE_KERNEL_OBJS) arch/x86_64/linker_base.ld
	$(LD) -m elf_i386 -T arch/x86_64/linker_base.ld -nostdlib -o $@ $(BASE_KERNEL_OBJS)

$(BASE_KERNEL_BIN): $(BASE_KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

arch/x86_64/boot_base.o: arch/x86_64/boot_base.S
	$(AS) $(ASFLAGS) -o $@ $<

kernels/kernel_template.o: kernels/kernel_template.c
	$(CC) $(CFLAGS) -I. -c -o $@ $<

# ─── Checks ────────────────────────────────────────────────────

check: check-multiboot check-size

check-multiboot:
	@echo "[CHECK] Verifying Multiboot header in $(HYPERKERNEL_ELF)..."
	@MAGIC=$$(objdump -s -j .multiboot $(HYPERKERNEL_ELF) 2>/dev/null | grep -o '02b0ad1b' | head -1); \
	if [ "$$MAGIC" != "02b0ad1b" ]; then \
		echo "  FAIL: Multiboot magic 0x1BADB002 not found!"; \
		exit 1; \
	fi; \
	echo "  PASS: Multiboot header present"

check-size:
	@echo "[CHECK] Verifying $(HYPERKERNEL_BIN) size..."
	@SIZE=$$(stat -c%s $(HYPERKERNEL_BIN) 2>/dev/null || echo 999999); \
	if [ $$SIZE -gt 51200 ]; then \
		echo "  FAIL: $$SIZE bytes (max 51200)"; \
		exit 1; \
	fi; \
	echo "  PASS: $$SIZE bytes (limit 51200)"

# ─── QEMU / ISO ────────────────────────────────────────────────

QEMU      ?= qemu-system-i386
GRUB_MKRESCUE ?= grub-mkrescue
XORRISO   ?= xorriso

iso: $(HYPERKERNEL_ELF)
	rm -rf iso tesseract.iso
	mkdir -p iso/boot/grub
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Tesseract Hyperkernel" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/hyperkernel.elf' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	cp $(HYPERKERNEL_ELF) iso/boot/hyperkernel.elf
	$(GRUB_MKRESCUE) --xorriso=$(XORRISO) -o tesseract.iso iso
	rm -rf iso

qemu: iso
	$(QEMU) -cdrom tesseract.iso -serial stdio

# ─── Clean ─────────────────────────────────────────────────────

clean:
	rm -f $(HYPERKERNEL_OBJS) $(HYPERKERNEL_ELF) $(HYPERKERNEL_BIN)
	rm -f $(BASE_KERNEL_OBJS) $(BASE_KERNEL_ELF) $(BASE_KERNEL_BIN)
	rm -f $(BASE_KERNEL_EMBED)
	rm -f /tmp/mb_check.bin tesseract.iso
