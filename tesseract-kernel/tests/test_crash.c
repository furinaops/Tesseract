/*
 * test_crash.c - Verify kernel crash detection
 *
 * This test verifies that:
 *   1. A kernel instance can be spawned
 *   2. When the kernel crashes (triple fault via `ud2`), the Hyperkernel
 *      detects it via heartbeat loss
 *   3. The Hyperkernel destroys the zombie kernel
 *   4. The Hyperkernel can spawn a replacement
 *
 * The test kernel intentionally executes an invalid instruction.
 * The Hyperkernel should detect the missing heartbeat and respond.
 *
 * Expected output:
 *   [HYPERKERNEL] Kernel 1 is zombie, destroying...
 */

__attribute__((noreturn)) void test_crash(void) {
    asm volatile("ud2");
    __builtin_unreachable();
}
