#!/bin/bash
#
# run_tests.sh — Run all vulnerability tests against the hyperkernel
#
# Each test:
#   1. Builds a malicious kernel image
#   2. Embeds it in the hyperkernel
#   3. Boots in QEMU for $TIMEOUT seconds
#   4. Captures serial output
#   5. Reports PASS/FAIL verdict based on expected behavior
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
TIMEOUT=12
PASS=0
FAIL=0

mkdir -p "$SCRIPT_DIR/results"

run_one_test() {
    local name="$1"
    local expected="$2"
    local unexpected="$3"
    local verdict_desc="$4"

    echo ""
    echo "=============================================="
    echo "  TEST: $name"
    echo "=============================================="

    # Build test kernel + hyperkernel
    "$SCRIPT_DIR/build_test.sh" "$name" 2>&1 | tail -3

    # Run QEMU and capture output
    echo "  Running QEMU (${TIMEOUT}s timeout)..."
    timeout "$TIMEOUT" qemu-system-i386 \
        -cdrom "$PROJ_DIR/tesseract.iso" \
        -nographic -no-reboot -serial mon:stdio \
        2>&1 > "$SCRIPT_DIR/results/${name}.txt" || true

    # Analyze output
    echo "  --- Captured output (tail) ---"
    tail -10 "$SCRIPT_DIR/results/${name}.txt"
    echo "  ---"

    # Check for expected patterns
    if grep -q "$expected" "$SCRIPT_DIR/results/${name}.txt" 2>/dev/null; then
        echo "  [PASS] Found expected: '$expected'"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] Missing expected: '$expected'"
        FAIL=$((FAIL + 1))
    fi

    # Check for unexpected patterns (if specified)
    if [ -n "$unexpected" ]; then
        if grep -q "$unexpected" "$SCRIPT_DIR/results/${name}.txt" 2>/dev/null; then
            echo "  [FAIL] Found unexpected: '$unexpected'"
            FAIL=$((FAIL + 1))
        else
            echo "  [PASS] No unexpected: '$unexpected'"
            PASS=$((PASS + 1))
        fi
    fi

    echo "  Verdict: $verdict_desc"
    echo ""
}

# ─── Test 1: Buffer Overflow ───────────────────────────────────
echo "=== TEST SUITE: Vulnerability Testing ==="
echo ""

run_one_test "overflow" \
    "PF " \
    "SURVIVED" \
    "Hyperkernel should catch the page fault and destroy the instance."

# ─── Test 2: Dirty COW (shared PDE 0-3) ────────────────────────
run_one_test "dirtycow" \
    "FAILED" \
    "PF " \
    "Instance should successfully write to another instance's memory via shared identity map."

# ─── Test 3: DoS via rapid syscalls ─────────────────────────────
run_one_test "dos" \
    "ret=" \
    "PF @0x10010000" \
    "Rate limiter rejects rapid syscalls (ret=-1), system stays stable during attack. Trailing PF after idle may occur."

# ─── Summary ────────────────────────────────────────────────────
echo "=============================================="
echo "  TEST SUMMARY"
echo "=============================================="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo "  Results: $SCRIPT_DIR/results/"
echo ""

# Restore production kernel
"$SCRIPT_DIR/restore.sh"

if [ "$FAIL" -eq 0 ]; then
    echo "  ALL TESTS PASSED"
else
    echo "  SOME TESTS FAILED (see above)"
    exit 1
fi
