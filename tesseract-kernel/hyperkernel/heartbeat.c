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

uint64_t heartbeat_get_tick(void) {
    return g_state.tick_count;
}

void heartbeat_tick(void) {
    g_state.tick_count++;
}

int heartbeat_register(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;
    inst->last_heartbeat = g_state.tick_count;
    return 0;
}

int heartbeat_check(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;
    if (g_state.tick_count - inst->last_heartbeat > HEARTBEAT_TIMEOUT_MS / 10) {
        inst->status = INSTANCE_ZOMBIE;
        return 0;
    }
    return 1;
}

void heartbeat_check_all(void) {
    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].status == INSTANCE_RUNNING ||
            g_state.instances[i].status == INSTANCE_SLEEPING) {
            if (heartbeat_check(g_state.instances[i].id) == 0) {
                hb_write("Heartbeat timeout: destroying instance ");
                uint32_t id = g_state.instances[i].id;
                char buf[16];
                int p = 0;
                if (id == 0) { buf[p++] = '0'; }
                else {
                    char rev[16];
                    int rp = 0;
                    while (id) { rev[rp++] = '0' + (id % 10); id /= 10; }
                    while (rp) buf[p++] = rev[--rp];
                }
                buf[p] = '\0';
                hb_write(buf);
                hb_write("\n");
                destroy_instance(g_state.instances[i].id);
            }
        }
    }
}
