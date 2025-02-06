#ifndef IPC_SERVICE_NRF_COMMON_H
#define IPC_SERVICE_NRF_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define BIT(n) (1UL << (n))
#define BIT_MASK(n) (BIT(n) - 1UL)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define __ASSERT_NO_MSG assert

#define BUILD_ASSERT(EXPR, MSG...)

/* Check if a pointer is aligned for against a specific byte boundary  */
#define IS_PTR_ALIGNED_BYTES(ptr, bytes) ((((uintptr_t)ptr) % bytes) == 0)

#define ROUND_UP(x, align)                                  \
    (((unsigned long)(x) + ((unsigned long)(align) - 1)) &  \
     ~((unsigned long)(align) - 1))

/* TODO: Placeholder */
static inline void sys_cache_data_flush_range(void *addr, size_t size) { (void) addr; (void) size; return; }
static inline void sys_cache_data_invd_range(void *addr, size_t size) { (void) addr; (void) size; return; }

/** Driver state. */
typedef enum
{
    DRV_STATE_UNINITIALIZED, ///< Uninitialized.
    DRV_STATE_INITIALIZED,   ///< Initialized but powered off.
    DRV_STATE_POWERED_ON,    ///< Initialized and powered on.
} drv_state_t;

#define BITMASK_BYTE_GET(abs_bit) ((abs_bit)/8)
#define BITMASK_RELBIT_GET(abs_bit) ((abs_bit) & 0x7UL)

static inline void bitmask_bit_clear(uint32_t bit, void *p_mask)
{
    uint8_t * p_mask8 = (uint8_t *) p_mask;
    uint32_t byte_idx = BITMASK_BYTE_GET(bit);
    bit = BITMASK_RELBIT_GET(bit);
    p_mask8[byte_idx] &= (uint8_t) ~(1U << bit);
}

static inline uint16_t sys_get_be16(const uint8_t src[2])
{
    return ((uint16_t)src[0] << 8) | src[1];
}

static inline void sys_put_be16(uint16_t val, uint8_t dst[2])
{
    dst[0] = val >> 8;
    dst[1] = val;
}

#endif /* IPC_SERVICE_NRF_COMMON_H */
