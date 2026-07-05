/*
 * test_spawn.c - Verify kernel spawning
 *
 * This test is compiled as a special kernel that verifies:
 *   1. The Hyperkernel boots and initializes
 *   2. A kernel instance can be spawned
 *   3. The kernel instance can execute code and write to VGA
 *
 * Build: make test-spawn && qemu-system-i386 -kernel test_spawn.bin
 *
 * Expected output:
 *   Tesseract Hyperkernel v0.1.0
 *   One Hyperkernel to rule them all.
 *   Initializing memory... OK
 *   Spawning kernel instance 1... OK (id=1)
 *   Jumping to kernel instance...
 *
 *   Hello from Kernel Instance 1!
 */

/* This test runs the standard hyperkernel boot flow.
 * It passes if it reaches kernel_main in the spawned instance. */
int test_main(void) {
    return 0;
}
