#!/bin/bash
#
# build_test.sh — Build hyperkernel with a vulnerability test kernel
#
# Usage: ./build_test.sh <test_name>
#   test_name: overflow | dirtycow | dos
#
# The script compiles the test kernel, replaces the default base_kernel.img,
# rebuilds the hyperkernel, and creates a bootable ISO.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"

TEST_NAME="$1"
if [ -z "$TEST_NAME" ]; then
    echo "Usage: $0 <test_name>"
    echo "  test_name: overflow | dirtycow | dos"
    exit 1
fi

TEST_SRC="$SCRIPT_DIR/test_${TEST_NAME}.c"
if [ ! -f "$TEST_SRC" ]; then
    echo "ERROR: Test source not found: $TEST_SRC"
    exit 1
fi

echo "=== Building test kernel: $TEST_NAME ==="

BUILD_DIR="$SCRIPT_DIR/build/$TEST_NAME"
mkdir -p "$BUILD_DIR"

CC="gcc"
LD="ld"
OBJCOPY="objcopy"
CFLAGS="-ffreestanding -nostdlib -nostartfiles -nodefaultlibs -m32 -mno-sse -mno-mmx -mno-80387 -fno-stack-protector -fno-pic -fno-pie -Wall -Wextra -Werror -O2 -g -I$PROJ_DIR -I$PROJ_DIR/hyperkernel -I$PROJ_DIR/libc/include"
LDFLAGS="-m elf_i386 -T $PROJ_DIR/arch/x86_64/linker_base.ld -nostdlib"

# Compile boot_base.S
echo "  Assembling boot_base.S..."
"$CC" -ffreestanding -nostdlib -m32 -g -c -o "$BUILD_DIR/boot_base.o" "$PROJ_DIR/arch/x86_64/boot_base.S"

# Compile test kernel
echo "  Compiling $TEST_SRC..."
"$CC" $CFLAGS -c -o "$BUILD_DIR/test_kernel.o" "$TEST_SRC"

# Link test kernel ELF
echo "  Linking test kernel ELF..."
"$LD" $LDFLAGS -o "$BUILD_DIR/test_kernel.elf" "$BUILD_DIR/boot_base.o" "$BUILD_DIR/test_kernel.o"

# Convert to flat binary
echo "  Generating flat binary..."
"$OBJCOPY" -O binary "$BUILD_DIR/test_kernel.elf" "$BUILD_DIR/test_kernel.bin"

# Replace base_kernel.img in the project (relative path for correct symbols)
echo "  Replacing kernels/base_kernel.img..."
cp "$BUILD_DIR/test_kernel.bin" "$PROJ_DIR/kernels/base_kernel.img"

# Re-embed as object file — run from PROJ_DIR so symbols match
echo "  Re-embedding as object..."
cd "$PROJ_DIR"
"$OBJCOPY" --input-target binary --output-target elf32-i386 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    kernels/base_kernel.img kernels/base_kernel_embed.o

# Rebuild hyperkernel
echo "  Rebuilding hyperkernel..."
make -C "$PROJ_DIR" hyperkernel 2>&1 | tail -5

# Build ISO
echo "  Building ISO..."
make -C "$PROJ_DIR" iso 2>&1 | tail -3

echo ""
echo "=== Build complete: $TEST_NAME ==="
echo "Run with: qemu-system-i386 -cdrom $PROJ_DIR/tesseract.iso -serial stdio -no-reboot"
