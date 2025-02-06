#ifndef IPC_SERVICE_BELLBOARD_H
#define IPC_SERVICE_BELLBOARD_H

#include <linux/stdint.h>
#include "core_vpr.h"
#include "wifi_mdk_common.h"
#include "riscv_encoding.h"

#ifdef NRF_LMAC
    #define CLIC (NRF_LMAC_VPRCLIC->CLIC)
#else
    #define CLIC (NRF_UMAC_VPRCLIC->CLIC)
#endif

typedef void (*bellboard_event_handler_t)(uint8_t event_idx, void *p_context);

/** @brief Structure for the BELLBOARD driver instance. */
typedef struct
{
    uint8_t  drv_inst_idx; ///< Index of the driver instance. For internal use only.
    uint8_t  int_idx;      ///< Interrupt index. For internal use only.
} nrfx_bellboard_t;

int bellboard_init(nrfx_bellboard_t const *p_instance, uint8_t interrupt_priority, bellboard_event_handler_t event_handler, void *p_context);
void bellboard_uninit(nrfx_bellboard_t const *p_instance);
void bellboard_int_enable(nrfx_bellboard_t const *p_instance, uint32_t mask);
void bellboard_int_disable(nrfx_bellboard_t const *p_instance, uint32_t mask);
uint32_t bellboard_int_pending_get(NRF_BELLBOARD_Type const *p_reg, uint8_t group_idx);
void bellboard_irq_handler(void);

#endif /* IPC_SERVICE_BELLBOARD_H */