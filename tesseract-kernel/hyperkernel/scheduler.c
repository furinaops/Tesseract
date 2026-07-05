#include "hyperkernel.h"

static uint32_t g_current = 0;

void scheduler_init(void) {
    g_current = 0;
}

int scheduler_next(void) {
    if (g_state.num_instances == 0) return -1;

    uint32_t started = g_current;
    do {
        g_current = (g_current + 1) % MAX_KERNEL_INSTANCES;
        if (g_state.instances[g_current].status == INSTANCE_RUNNING) {
            return (int)g_state.instances[g_current].id;
        }
    } while (g_current != started);

    return -1;
}

int scheduler_get_current(void) {
    return (int)g_state.instances[g_current].id;
}
