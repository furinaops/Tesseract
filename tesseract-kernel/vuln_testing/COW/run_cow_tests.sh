#!/bin/bash
# run_cow_tests.sh — Build and run all 5 Dirty-COW-style tests
#
# Expected results after restricted PDE 0 fix:
#   COW1 (PDE 1, 0x400000):   PAGE FAULT  → PF caught, instance destroyed
#   COW2 (PDE 2, 0x800000):   PAGE FAULT  → PF caught, instance destroyed
#   COW3 (PDE 3, 0xC00000):   PAGE FAULT  → PF caught, instance destroyed
#   COW4 (code page 0x108000): WRITE OK    → code pages are RW-mapped
#   COW5 (g_state 0x108fa0):   WRITE OK    → g_state is in RW-mapped region

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TIMEOUT=12

mkdir -p "$SCRIPT_DIR/results"

build_one() {
    local name="$1"
    local src="$SCRIPT_DIR/test_${name}.c"

    if [ ! -f "$src" ]; then
        echo "  ERROR: $src not found"
        exit 1
    fi

    local bdir="$SCRIPT_DIR/build/$name"
    mkdir -p "$bdir"

    gcc -ffreestanding -nostdlib -m32 -g -c -o "$bdir/boot_base.o" \
        "$PROJ_DIR/arch/x86_64/boot_base.S"

    gcc -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -m32 \
        -mno-sse -mno-mmx -mno-80387 -fno-stack-protector -fno-pic -fno-pie \
        -Wall -Wextra -Werror -O2 -g \
        -I"$PROJ_DIR" -I"$PROJ_DIR/hyperkernel" -I"$PROJ_DIR/libc/include" \
        -c -o "$bdir/test_kernel.o" "$src"

    ld -m elf_i386 -T "$PROJ_DIR/arch/x86_64/linker_base.ld" -nostdlib \
        -o "$bdir/test_kernel.elf" "$bdir/boot_base.o" "$bdir/test_kernel.o"

    objcopy -O binary "$bdir/test_kernel.elf" "$bdir/test_kernel.bin"

    cp "$bdir/test_kernel.bin" "$PROJ_DIR/kernels/base_kernel.img"

    cd "$PROJ_DIR"
    objcopy --input-target binary --output-target elf32-i386 \
        --rename-section .data=.rodata,alloc,load,readonly,data,contents \
        kernels/base_kernel.img kernels/base_kernel_embed.o

    make -C "$PROJ_DIR" hyperkernel 2>&1 | tail -1
    make -C "$PROJ_DIR" iso 2>&1 | tail -1
}

run_one() {
    local name="$1"
    local expected="$2"
    local unexpected="$3"
    local desc="$4"

    echo ""
    echo "=============================================="
    echo "  TEST: $name"
    echo "=============================================="

    build_one "$name" 2>&1 | tail -1

    echo "  Running QEMU (${TIMEOUT}s)..."
    timeout "$TIMEOUT" qemu-system-i386 \
        -cdrom "$PROJ_DIR/tesseract.iso" \
        -nographic -no-reboot -serial mon:stdio \
        2>&1 > "$SCRIPT_DIR/results/${name}.txt" || true

    echo "  --- Captured output (tail -12) ---"
    tail -12 "$SCRIPT_DIR/results/${name}.txt"
    echo "  ---"

    local passed=0
    local failed=0

    if grep -q "$expected" "$SCRIPT_DIR/results/${name}.txt" 2>/dev/null; then
        echo "  [PASS] Found expected: '$expected'"
        passed=$((passed + 1))
    else
        echo "  [FAIL] Missing expected: '$expected'"
        failed=$((failed + 1))
    fi

    if [ -n "$unexpected" ]; then
        if grep -q "$unexpected" "$SCRIPT_DIR/results/${name}.txt" 2>/dev/null; then
            echo "  [FAIL] Found unexpected: '$unexpected'"
            failed=$((failed + 1))
        else
            echo "  [PASS] No unexpected: '$unexpected'"
            passed=$((passed + 1))
        fi
    fi

    echo "  Verdict: $desc"
    echo "  Checks: $passed passed, $failed failed"
}

echo "=== COW TEST SUITE: Cross-instance Memory Write Tests ==="
echo ""

# COW1: PDE 1 write → should page-fault
run_one "cow1" \
    "PF 00400000" \
    "SUCCEEDED" \
    "PDE 1 (0x400000) is unmapped; write should page-fault."

# COW2: PDE 2 write → should page-fault
run_one "cow2" \
    "PF 00800000" \
    "SUCCEEDED" \
    "PDE 2 (0x800000) is unmapped; write should page-fault."

# COW3: PDE 3 write → should page-fault
run_one "cow3" \
    "PF 00C00000" \
    "SUCCEEDED" \
    "PDE 3 (0xC00000) is unmapped; write should page-fault."

# COW4: code page write → should page-fault (restricted PDE 0)
run_one "cow4" \
    "PF 00108000" \
    "SUCCEEDED" \
    "Code pages (0x108000) are supervisor-only; write should page-fault."

# COW5: g_state write → should page-fault (restricted PDE 0)
run_one "cow5" \
    "PF 00108FA0" \
    "SUCCEEDED" \
    "g_state (0x108fa0) is in supervisor-only region; write should page-fault."

echo ""
echo "=============================================="
echo "  COW TEST SUMMARY"
echo "=============================================="
echo "  Results: $SCRIPT_DIR/results/"
echo ""

# Restore production kernel
echo "Restoring production kernel..."
bash "$PROJ_DIR/vuln_testing/restore.sh" 2>&1 | tail -1
echo "=== Production kernel restored ==="
