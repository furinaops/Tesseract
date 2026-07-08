#include "hyperkernel.h"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

typedef int (*syscall_handler_t)(uint32_t, uint32_t, uint32_t);

static int sys_kspawn(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    int current = scheduler_get_current();
    if (current < 0) return -1;
    kernel_instance_t *inst = get_instance((uint32_t)current);
    if (!inst) return -1;

    if (g_state.tick_count - inst->last_spawn_tick < 100)
        return -1;
    inst->last_spawn_tick = (uint32_t)g_state.tick_count;

    return spawn_instance(a, PRIORITY_NORMAL, 0);
}

static int sys_kdestroy(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    int current = scheduler_get_current();
    if (current < 0) return -1;
    if ((uint32_t)current != a) return -1;
    return destroy_instance(a);
}

static int sys_kping(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return heartbeat_register(a, 0);
}

static int sys_klog(uint32_t a, uint32_t b, uint32_t c) {
    (void)c;
    int current = scheduler_get_current();
    if (current < 0) return -1;
    kernel_instance_t *inst = get_instance((uint32_t)current);
    if (!inst) return -1;

    if (!((b >= 0xB8000 && b < 0xB9000) ||
          (b >= inst->memory_base && b < inst->memory_base + KERNEL_INSTANCES_SIZE)))
        return -1;

    terminal_writestring("[KERNEL ");
    terminal_writedec(a);
    terminal_writestring("] ");
    terminal_writestring((const char *)(uintptr_t)b);
    terminal_writestring("\n");
    return 0;
}

static int sys_heartbeat(uint32_t a, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    int current = scheduler_get_current();
    if (current < 0) return -1;
    kernel_instance_t *inst = get_instance((uint32_t)current);
    if (!inst) return -1;

    uint32_t ticks_since_last = (uint32_t)(g_state.tick_count - inst->last_request_tick);
    if (ticks_since_last < 5) {
        return -1;
    }
    inst->last_request_tick = (uint32_t)g_state.tick_count;

    if (a != 0) {
        const heartbeat_payload_t *payload = (const heartbeat_payload_t *)(uintptr_t)a;
        if (payload->memory_request > MAX_MEMORY_REQUEST) return -1;
        if (payload->health_score > 100) return -1;
        inst->memory_requested = payload->memory_request;
        inst->health_score = payload->health_score;
        inst->last_syscall_tick = g_state.tick_count;
        inst->consecutive_misses = 0;
        if (inst->stage == STAGE_FLAGGED) {
            instance_thaw(inst->id);
        }
    } else {
        inst->last_syscall_tick = g_state.tick_count;
        inst->consecutive_misses = 0;
        if (inst->stage == STAGE_FLAGGED) {
            instance_thaw(inst->id);
        }
    }
    inst->last_heartbeat = g_state.tick_count;
    return 0;
}

static int sys_serial(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    outb(0x3F8, (uint8_t)a);
    return 0;
}

static syscall_handler_t g_syscall_table[8] = {
    0,              /* 0 - unused */
    sys_kspawn,     /* 1 */
    sys_kdestroy,   /* 2 */
    sys_kping,      /* 3 */
    sys_klog,       /* 4 */
    sys_heartbeat,  /* 5 */
    sys_serial,     /* 6 */
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
