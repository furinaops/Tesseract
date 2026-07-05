#include "hyperkernel.h"

#define LOG_RING_SIZE 256

typedef struct {
    uint32_t kernel_id;
    uint32_t timestamp;
    char     message[64];
} log_entry_t;

static log_entry_t g_log_ring[LOG_RING_SIZE];
static uint32_t    g_log_idx;

void log_init(void) {
    g_log_idx = 0;
}

void log_write(uint32_t kernel_id, const char *msg) {
    log_entry_t *entry = &g_log_ring[g_log_idx % LOG_RING_SIZE];
    entry->kernel_id = kernel_id;
    entry->timestamp = heartbeat_get_tick();

    int i = 0;
    while (msg[i] && i < (int)sizeof(entry->message) - 1) {
        entry->message[i] = msg[i];
        i++;
    }
    entry->message[i] = '\0';

    g_log_idx++;
}

void log_dump(void) {
    uint32_t start = g_log_idx > LOG_RING_SIZE ? g_log_idx - LOG_RING_SIZE : 0;
    for (uint32_t i = start; i < g_log_idx; i++) {
        log_entry_t *entry = &g_log_ring[i % LOG_RING_SIZE];
        terminal_writestring("[");
        terminal_writedec(entry->timestamp);
        terminal_writestring("] K");
        terminal_writedec(entry->kernel_id);
        terminal_writestring(": ");
        terminal_writestring(entry->message);
        terminal_writestring("\n");
    }
}
