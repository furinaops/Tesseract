#include "hyperkernel.h"

extern uint8_t _binary_kernels_base_kernel_img_start[];
extern uint8_t _binary_kernels_base_kernel_img_end[];

#define KERNEL_LOAD_BASE  0x1000000
#define KERNEL_TOTAL_SIZE (64 * 1024)

uint32_t get_base_kernel_size(void) {
    return (uint32_t)(_binary_kernels_base_kernel_img_end -
                      _binary_kernels_base_kernel_img_start);
}

uint32_t get_base_kernel_entry(void) {
    return KERNEL_LOAD_BASE;
}

int load_base_kernel(uint32_t dest_addr) {
    uint32_t code_size = get_base_kernel_size();
    if (code_size == 0) return -1;

    uint8_t *src = _binary_kernels_base_kernel_img_start;
    uint8_t *dst = (uint8_t *)(uintptr_t)dest_addr;

    for (uint32_t i = 0; i < code_size; i++) {
        dst[i] = src[i];
    }

    for (uint32_t i = code_size; i < KERNEL_TOTAL_SIZE; i++) {
        dst[i] = 0;
    }

    return 0;
}
