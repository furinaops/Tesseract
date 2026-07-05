#include "hyperkernel.h"

extern int load_base_kernel(uint32_t dest_addr);
extern uint32_t get_base_kernel_size(void);
extern uint32_t get_base_kernel_entry(void);

static uint32_t g_next_id = 1;

int spawn_instance(uint32_t kernel_image_id, priority_t priority, uint32_t user_id) {
    (void)kernel_image_id;

    if (g_state.num_instances >= MAX_KERNEL_INSTANCES) {
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].status == INSTANCE_FREE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    uint32_t load_addr = KERNEL_INSTANCES_BASE +
                         (uint32_t)slot * KERNEL_INSTANCES_STRIDE;

    if (load_base_kernel(load_addr) != 0) {
        return -1;
    }

    uint32_t pd = paging_create_instance_dir(load_addr);
    if (!pd) {
        return -1;
    }

    kernel_instance_t *inst = &g_state.instances[slot];
    inst->id            = g_next_id++;
    inst->status        = INSTANCE_RUNNING;
    inst->entry_point   = get_base_kernel_entry();
    inst->memory_base   = load_addr;
    inst->memory_size   = KERNEL_INSTANCES_SIZE;
    inst->stack_pointer = load_addr + KERNEL_INSTANCES_STRIDE - 16;
    inst->page_directory = pd;
    inst->last_heartbeat = g_state.tick_count;
    inst->ticks_run      = 0;

    inst->priority          = priority;
    inst->stage             = STAGE_HEALTHY;
    inst->user_id           = user_id;
    inst->memory_requested  = KERNEL_INSTANCES_SIZE;
    inst->memory_allocated  = KERNEL_INSTANCES_SIZE;
    inst->health_score      = 100;
    inst->last_syscall_tick = g_state.tick_count;
    inst->consecutive_misses = 0;
    inst->last_request_tick = 0;

    uint32_t id_addr_phys = load_addr + 0xFFC0;
    *(uint32_t *)(uintptr_t)id_addr_phys = inst->id;

    uint32_t frame_phys = load_addr + KERNEL_INSTANCES_STRIDE - 60;
    uint32_t *frame = (uint32_t *)(uintptr_t)frame_phys;
    frame[0]  = 0x10;   // gs
    frame[1]  = 0x10;   // fs
    frame[2]  = 0x10;   // es
    frame[3]  = 0x10;   // ds
    frame[4]  = 0;      // edi
    frame[5]  = 0;      // esi
    frame[6]  = 0;      // ebp
    frame[7]  = 0;      // original_esp (skipped by popal)
    frame[8]  = 0;      // ebx
    frame[9]  = 0;      // edx
    frame[10] = 0;      // ecx
    frame[11] = 0;      // eax
    frame[12] = inst->entry_point;  // EIP
    frame[13] = 0x08;               // CS
    frame[14] = 0x202;              // EFLAGS
    inst->saved_esp = 0x1000000 + KERNEL_INSTANCES_STRIDE - 60;

    g_state.num_instances++;
    return (int)inst->id;
}

static void free_instance_dir(uint32_t pd_phys) {
    if (!pd_phys) return;
    uint32_t *pd = (uint32_t *)(uintptr_t)pd_phys;
    uint32_t pt4_phys = pd[4] & ~0xFFF;
    if (pt4_phys) free_page((void *)(uintptr_t)pt4_phys);
    free_page(pd);
}

int instance_freeze(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;
    inst->stage = STAGE_DEPRECATED;
    inst->memory_allocated = 0;
    return 0;
}

int instance_thaw(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;
    inst->stage = STAGE_HEALTHY;
    inst->memory_allocated = KERNEL_INSTANCES_SIZE;
    inst->consecutive_misses = 0;
    return 0;
}

int instance_execute(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;
    inst->stage = STAGE_EXECUTED;
    inst->status = INSTANCE_ZOMBIE;

    uint32_t base = inst->memory_base;
    uint32_t *ptr = (uint32_t *)(uintptr_t)base;
    for (uint32_t i = 0; i < KERNEL_INSTANCES_SIZE / 4; i++) {
        ptr[i] = 0;
    }

    free_instance_dir(inst->page_directory);
    inst->page_directory = 0;
    inst->memory_allocated = 0;
    g_state.num_instances--;
    return 0;
}

int destroy_instance(uint32_t kernel_id) {
    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].id == kernel_id &&
            g_state.instances[i].status != INSTANCE_FREE) {
            free_instance_dir(g_state.instances[i].page_directory);
            g_state.instances[i].status = INSTANCE_FREE;
            g_state.instances[i].id = 0;
            g_state.instances[i].page_directory = 0;
            g_state.num_instances--;
            return 0;
        }
    }
    return -1;
}

kernel_instance_t *get_instance(uint32_t kernel_id) {
    for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
        if (g_state.instances[i].id == kernel_id &&
            g_state.instances[i].status != INSTANCE_FREE) {
            return &g_state.instances[i];
        }
    }
    return 0;
}

int jump_to_kernel(uint32_t kernel_id) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;

    inst->status = INSTANCE_RUNNING;

    if (inst->page_directory) {
        paging_switch(inst->page_directory);
    }

    void (*kernel_entry)(void) = (void (*)(void))(uintptr_t)inst->entry_point;
    kernel_entry();

    return 0;
}
