#include "hyperkernel.h"

static uint8_t hb_inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void hb_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void hb_putchar(char c) {
    while ((hb_inb(0x3F8 + 5) & 0x20) == 0);
    hb_outb(0x3F8, (uint8_t)c);
}

static void hb_write(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') hb_putchar('\r');
        hb_putchar(str[i]);
    }
}

static void hb_writedec(uint32_t n) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (n == 0) { hb_putchar('0'); return; }
    while (n > 0 && i > 0) {
        i--;
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    hb_write(&buf[i]);
}

uint64_t heartbeat_get_tick(void) {
    return g_state.tick_count;
}

void heartbeat_tick(void) {
    g_state.tick_count++;
}

int heartbeat_register(uint32_t kernel_id, const heartbeat_payload_t *payload) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;

    if (payload) {
        inst->memory_requested = payload->memory_request;
        inst->health_score = payload->health_score;
    }
    inst->last_heartbeat = g_state.tick_count;
    inst->last_syscall_tick = g_state.tick_count;
    inst->consecutive_misses = 0;

    if (inst->stage == STAGE_FLAGGED) {
        instance_thaw(inst->id);
    }
    return 0;
}

uint32_t compute_heartbeat_timeout(void) {
    uint32_t base = 100;
    uint32_t load_extension = (g_state.load_factor * 30) / 1000;
    if (load_extension > 50) load_extension = 50;
    return base + load_extension;
}

int heartbeat_check(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;

    uint32_t timeout = compute_heartbeat_timeout();
    if (g_state.tick_count - inst->last_heartbeat > timeout) {
        return 0;
    }
    return 1;
}

void heartbeat_process_escalation(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return;
    if (inst->status != INSTANCE_RUNNING) return;

    uint32_t timeout = compute_heartbeat_timeout();
    uint64_t elapsed = g_state.tick_count - inst->last_heartbeat;

    switch (inst->stage) {
    case STAGE_HEALTHY:
        if (elapsed > timeout) {
            inst->consecutive_misses++;
            if (inst->consecutive_misses >= 1) {
                inst->stage = STAGE_FLAGGED;
                inst->memory_allocated = 0;
                hb_write("FLAG ");
                hb_writedec(inst->id);
                hb_write("\n");
            }
        }
        if (inst->memory_requested > MAX_MEMORY_REQUEST) {
            inst->stage = STAGE_FLAGGED;
            inst->memory_allocated = 0;
            hb_write("FLAG ");
            hb_writedec(inst->id);
            hb_write(" (mem)\n");
        }
        break;

    case STAGE_FLAGGED:
        if (elapsed > timeout) {
            inst->consecutive_misses++;
            if (inst->consecutive_misses >= 2) {
                instance_freeze(inst->id);
                inst->memory_allocated = 0;
                hb_write("FREEZE ");
                hb_writedec(inst->id);
                hb_write("\n");
            }
        }
        break;

    case STAGE_DEPRECATED:
        if (elapsed > timeout) {
            inst->consecutive_misses++;
            if (inst->consecutive_misses >= 3) {
                instance_execute(inst->id);
                hb_write("EXECUTE ");
                hb_writedec(inst->id);
                hb_write("\n");
            }
        } else {
            inst->consecutive_misses = 0;
        }
        break;

    default:
        break;
    }
}

void heartbeat_check_all(void) {
    int deprecated_count = 0;

    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].status == INSTANCE_RUNNING ||
            g_state.instances[i].status == INSTANCE_SLEEPING) {
            if (heartbeat_check(g_state.instances[i].id) == 0) {
                hb_write("Heartbeat timeout: destroying instance ");
                hb_writedec(g_state.instances[i].id);
                hb_write("\n");
                destroy_instance(g_state.instances[i].id);
            }
        }
        if (g_state.instances[i].stage == STAGE_DEPRECATED) {
            deprecated_count++;
        }
    }

    if (deprecated_count > 3 && !g_state.emergency_mode) {
        hb_write("EMERGENCY: >3 instances deprecated. Entering safe mode.\n");
        g_state.emergency_mode = 1;
        for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
            if (g_state.instances[i].status != INSTANCE_FREE) {
                instance_execute(g_state.instances[i].id);
            }
        }
        recovery_kernel_entry();
    }
}

void arbitrate_memory(void) {
    uint32_t total_requested = 0;
    uint32_t normal_idle_count = 0;

    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].status == INSTANCE_RUNNING ||
            g_state.instances[i].status == INSTANCE_SLEEPING) {
            uint32_t req = g_state.instances[i].memory_requested;
            if (req > MAX_MEMORY_REQUEST) {
                req = MAX_MEMORY_REQUEST;
            }
            total_requested += req;

            if (g_state.instances[i].stage == STAGE_FLAGGED ||
                g_state.instances[i].stage == STAGE_DEPRECATED) {
                continue;
            }

            if (g_state.instances[i].priority > PRIORITY_HIGH) {
                normal_idle_count++;
            }
        }
    }

    if (total_requested <= g_state.total_memory) {
        for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
            if (g_state.instances[i].status == INSTANCE_RUNNING ||
                g_state.instances[i].status == INSTANCE_SLEEPING) {
                if (g_state.instances[i].stage == STAGE_FLAGGED ||
                    g_state.instances[i].stage == STAGE_DEPRECATED ||
                    g_state.instances[i].stage == STAGE_EXECUTED) {
                    g_state.instances[i].memory_allocated = 0;
                } else if (g_state.instances[i].memory_requested > MAX_MEMORY_REQUEST) {
                    g_state.instances[i].memory_allocated = MAX_MEMORY_REQUEST;
                } else {
                    g_state.instances[i].memory_allocated = g_state.instances[i].memory_requested;
                }
            }
        }
        return;
    }

    uint32_t remaining = g_state.total_memory;

    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].status == INSTANCE_RUNNING ||
            g_state.instances[i].status == INSTANCE_SLEEPING) {
            if (g_state.instances[i].stage == STAGE_FLAGGED ||
                g_state.instances[i].stage == STAGE_DEPRECATED ||
                g_state.instances[i].stage == STAGE_EXECUTED) {
                g_state.instances[i].memory_allocated = 0;
            } else if (g_state.instances[i].priority <= PRIORITY_HIGH) {
                uint32_t grant = g_state.instances[i].memory_requested;
                if (grant > MAX_MEMORY_REQUEST) grant = MAX_MEMORY_REQUEST;
                if (grant > remaining) grant = remaining;
                g_state.instances[i].memory_allocated = grant;
                remaining -= grant;
            }
        }
    }

    if (normal_idle_count > 0 && remaining > 0) {
        for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
            if ((g_state.instances[i].status == INSTANCE_RUNNING ||
                 g_state.instances[i].status == INSTANCE_SLEEPING) &&
                g_state.instances[i].priority > PRIORITY_HIGH &&
                g_state.instances[i].stage == STAGE_HEALTHY) {
                uint32_t share = remaining / normal_idle_count;
                uint32_t req = g_state.instances[i].memory_requested;
                if (req > MAX_MEMORY_REQUEST) req = MAX_MEMORY_REQUEST;
                g_state.instances[i].memory_allocated = (share < req) ? share : req;
                remaining -= g_state.instances[i].memory_allocated;
                normal_idle_count--;
                if (normal_idle_count > 0) {
                    share = remaining / normal_idle_count;
                }
            }
        }
    }
}
