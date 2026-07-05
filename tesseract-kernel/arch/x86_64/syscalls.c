#include "hyperkernel.h"

typedef int (*syscall_handler_t)(uint32_t, uint32_t, uint32_t);

static int sys_kspawn(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return spawn_instance(a);
}

static int sys_kdestroy(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return destroy_instance(a);
}

static int sys_kping(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return heartbeat_register(a);
}

static int sys_klog(uint32_t a, uint32_t b, uint32_t c) {
    (void)c;
    terminal_writestring("[KERNEL ");
    terminal_writedec(a);
    terminal_writestring("] ");
    terminal_writestring((const char *)(uintptr_t)b);
    terminal_writestring("\n");
    return 0;
}

static int sys_heartbeat(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    int current = scheduler_get_current();
    if (current < 0) return -1;
    kernel_instance_t *inst = get_instance((uint32_t)current);
    if (!inst) return -1;
    inst->last_heartbeat = g_state.tick_count;
    return 0;
}

static syscall_handler_t g_syscall_table[8] = {
    0,              /* 0 - unused */
    sys_kspawn,     /* 1 */
    sys_kdestroy,   /* 2 */
    sys_kping,      /* 3 */
    sys_klog,       /* 4 */
    sys_heartbeat,  /* 5 */
    0,              /* 6 */
    0,              /* 7 */
};

int syscall_handler_c(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    if (num < sizeof(g_syscall_table) / sizeof(g_syscall_table[0])) {
        syscall_handler_t handler = g_syscall_table[num];
        if (handler) {
            return handler(a, b, c);
        }
    }
    return -1;
}
