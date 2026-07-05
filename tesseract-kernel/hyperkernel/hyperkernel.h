#ifndef HYPERKERNEL_H
#define HYPERKERNEL_H

#include <stdint.h>
#include <stddef.h>

#define PANIC(msg) do { \
    terminal_writestring("PANIC: "); \
    terminal_writestring(msg); \
    terminal_writestring(" at " __FILE__ ":"); \
    asm volatile("cli; hlt"); \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        PANIC(msg); \
    } \
} while (0)

#define MAX_KERNEL_INSTANCES 16
#define KERNEL_STACK_SIZE    16384
#define HEARTBEAT_TIMEOUT_MS 3000
#define HYPERKERNEL_VERSION  "1.1.0"

#define MEMORY_POOL_SIZE     (16 * 1024 * 1024)
#define MAX_MEMORY_REQUEST   (MEMORY_POOL_SIZE * 60 / 100)

typedef enum {
    INSTANCE_FREE = 0,
    INSTANCE_RUNNING,
    INSTANCE_SLEEPING,
    INSTANCE_ZOMBIE,
    INSTANCE_CRASHED
} instance_status_t;

typedef enum {
    PRIORITY_CRITICAL = 0,
    PRIORITY_HIGH,
    PRIORITY_NORMAL,
    PRIORITY_IDLE
} priority_t;

typedef enum {
    STAGE_HEALTHY = 0,
    STAGE_FLAGGED,
    STAGE_DEPRECATED,
    STAGE_EXECUTED
} escalation_stage_t;

typedef struct {
    uint32_t kernel_id;
    uint32_t user_id;
} kernel_identity_t;

typedef struct {
    uint32_t memory_footprint;
    uint32_t memory_request;
    uint32_t health_score;
    uint32_t last_syscall_low;
} heartbeat_payload_t;

typedef struct {
    uint32_t    id;
    instance_status_t status;
    uint32_t    entry_point;
    uint32_t    memory_base;
    uint32_t    memory_size;
    uint32_t    stack_pointer;
    uint32_t    page_directory;
    uint64_t    last_heartbeat;
    uint32_t    ticks_run;
    uint32_t    saved_esp;

    priority_t         priority;
    escalation_stage_t stage;
    uint32_t           user_id;
    uint32_t           memory_requested;
    uint32_t           memory_allocated;
    uint32_t           health_score;
    uint64_t           last_syscall_tick;
    uint32_t           consecutive_misses;
    uint32_t           last_request_tick;
} kernel_instance_t;

typedef struct {
    kernel_instance_t instances[MAX_KERNEL_INSTANCES];
    uint32_t          num_instances;
    uint32_t          current_instance;
    uint64_t          tick_count;

    uint32_t          total_memory;
    uint32_t          load_factor;
    int               emergency_mode;
} hyperkernel_state_t;

extern hyperkernel_state_t g_state;

#define KERNEL_INSTANCES_BASE  0x1000000
#define KERNEL_INSTANCES_SIZE (64 * 1024)
#define KERNEL_INSTANCES_STRIDE (64 * 1024)

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_writestring(const char *str);
void terminal_writehex(uint32_t n);
void terminal_writedec(uint32_t n);
void terminal_setcolor(uint8_t color);

void memory_init(void);
void *alloc_page(void);
void free_page(void *addr);
void *alloc_pages(uint32_t count);
void free_pages(void *addr, uint32_t count);

int  spawn_instance(uint32_t kernel_image_id, priority_t priority, uint32_t user_id);
int  destroy_instance(uint32_t kernel_id);
int  instance_freeze(uint32_t kernel_id);
int  instance_thaw(uint32_t kernel_id);
int  instance_execute(uint32_t kernel_id);
kernel_instance_t *get_instance(uint32_t kernel_id);
int  jump_to_kernel(uint32_t kernel_id);

void scheduler_init(void);
int  scheduler_next(void);
int  scheduler_get_current(void);

uint64_t heartbeat_get_tick(void);
void heartbeat_tick(void);
int  heartbeat_register(uint32_t kernel_id, const heartbeat_payload_t *payload);
int  heartbeat_check(uint32_t kernel_id);
void heartbeat_check_all(void);
void heartbeat_process_escalation(uint32_t kernel_id);
uint32_t compute_heartbeat_timeout(void);
void arbitrate_memory(void);
int  paging_map_instance_page(uint32_t kernel_id, uint32_t vaddr, uint32_t phys);
int  paging_unmap_instance_page(uint32_t kernel_id, uint32_t vaddr);
void recovery_kernel_entry(void);

void interrupt_init(void);
void timer_init(void);
void patch_cr3_values(void);
void paging_init(void);
void paging_enable(void);
uint32_t paging_create_instance_dir(uint32_t instance_phys);
uint32_t paging_create_instance_dir_restricted(uint32_t instance_phys);
void paging_switch(uint32_t page_dir_phys);
uint32_t paging_get_hypervisor_dir(void);
void hypervisor_init_gdt(void);
int syscall_handler_c(uint32_t num, uint32_t a, uint32_t b, uint32_t c);

void log_init(void);
void log_write(uint32_t kernel_id, const char *msg);
void log_dump(void);

typedef struct {
    uint32_t state[8];
    uint32_t count[2];
    uint8_t  buffer[64];
} sha256_ctx_t;
void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]);
int  crypto_verify(const uint8_t *data, uint32_t len, const uint8_t expected_hash[32]);

void kernel_main(void);

#endif
