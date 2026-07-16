CC       := gcc
LD       := gcc
OBJCOPY  := objcopy
BASE_CFLAGS := -ffreestanding -nostdlib -nostartfiles \
            -nodefaultlibs -m32 -mno-sse -mno-mmx -mno-80387 \
            -fno-stack-protector -fno-pic -fno-pie \
            -Wall -Wextra -O2 -g \
            -nostdinc -I doominclude -I doom \
            -DCMAP256 -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200

# Our platform code uses Werror; DOOM sources have minor issues we ignore
CFLAGS   := $(BASE_CFLAGS) -Werror
DOOM_CFLAGS := $(BASE_CFLAGS) -Wno-error=missing-field-initializers \
            -Wno-error=unused-parameter -Wno-error=sign-compare \
            -Wno-error=implicit-fallthrough -Wno-error=unterminated-string-initialization
ASFLAGS  := -ffreestanding -nostdlib -m32 -g -c
LDFLAGS  := -m32 -T linker.ld -nostdlib -no-pie -static

KERNEL_ELF := demo.elf

DOOM_SRCS := $(wildcard doom/*.c)
DOOM_EXCLUDE := \
    doom/doomgeneric_allegro.c \
    doom/doomgeneric_sdl.c \
    doom/doomgeneric_xlib.c \
    doom/doomgeneric_linuxvt.c \
    doom/doomgeneric_win.c \
    doom/doomgeneric_soso.c \
    doom/doomgeneric_sosox.c \
    doom/doomgeneric_emscripten.c \
    doom/i_scale.c \
    doom/i_endoom.c \
    doom/i_sdlmusic.c \
    doom/i_allegromusic.c \
    doom/i_allegrosound.c \
    doom/i_sdlsound.c \
    doom/i_cdmus.c \
    doom/i_joystick.c \
    doom/icon.c \
    doom/sha1.c \
    doom/statdump.c \
    doom/gusconf.c \
    doom/mus2mid.c \
    doom/memio.c \
    doom/w_checksum.c
DOOM_SRCS := $(filter-out $(DOOM_EXCLUDE), $(DOOM_SRCS))
DOOM_OBJS := $(DOOM_SRCS:.c=.o)

PLATFORM_OBJS := dg_platform.o doom_libc.o

KERNEL_OBJS := boot.o kernel.o mini_py.o

ALL_OBJS := $(KERNEL_OBJS) $(PLATFORM_OBJS) $(DOOM_OBJS) doom1_wad.o

.PHONY: all clean qemu iso

all: $(KERNEL_ELF)

boot.o: boot.S
	$(CC) $(ASFLAGS) -o $@ $<

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

mini_py.o: mini_py.c mini_py.h
	$(CC) $(CFLAGS) -c -o $@ $<

dg_platform.o: dg_platform.c
	$(CC) $(CFLAGS) -c -o $@ $<

doom_libc.o: doom_libc.c
	$(CC) $(CFLAGS) -c -o $@ $<

doom1_wad.o: doom1.wad
	$(OBJCOPY) --input-target binary --output-target elf32-i386 \
	  --rename-section .data=.rodata $< $@

doom/%.o: doom/%.c
	$(CC) $(DOOM_CFLAGS) -c -o $@ $<

$(KERNEL_ELF): $(ALL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS) -lgcc

iso: $(KERNEL_ELF)
	rm -rf iso demo.iso
	mkdir -p iso/boot/grub
	printf 'set timeout=0\nset default=0\nmenuentry "Tesseract Demo" { multiboot /boot/demo.elf }\n' > iso/boot/grub/grub.cfg
	cp $(KERNEL_ELF) iso/boot/demo.elf
	grub-mkrescue --xorriso=xorriso -o demo.iso iso 2>/dev/null
	rm -rf iso

qemu: iso
	qemu-system-i386 -cdrom demo.iso -serial stdio

clean:
	rm -f $(ALL_OBJS) $(KERNEL_ELF) demo.iso
	rm -rf iso
