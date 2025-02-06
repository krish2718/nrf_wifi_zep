#ifndef IPC_SERVICE_VEVIF_H
#define IPC_SERVICE_VEVIF_H

#include <stdint.h>
#include "core_vpr.h"
#include "wifi_mdk_common.h"
#include "riscv_encoding.h"

typedef void (*vevif_event_handler_t)(uint8_t event_idx, void *p_context);

#ifdef NRF_LMAC
    #define CLIC (NRF_LMAC_VPRCLIC->CLIC)
#else
    #define CLIC (NRF_UMAC_VPRCLIC->CLIC)
#endif

#if defined(__RISCV__)
    /** @brief Interrupt priority level. */
    typedef enum
    {
        NRF_VPR_CLIC_PRIORITY_LEVEL0 = CLIC_CLIC_CLICINT_PRIORITY_PRIOLEVEL0, /**< Priority level 0. */
        NRF_VPR_CLIC_PRIORITY_LEVEL1 = CLIC_CLIC_CLICINT_PRIORITY_PRIOLEVEL1, /**< Priority level 1. */
        NRF_VPR_CLIC_PRIORITY_LEVEL2 = CLIC_CLIC_CLICINT_PRIORITY_PRIOLEVEL2, /**< Priority level 2. */
        NRF_VPR_CLIC_PRIORITY_LEVEL3 = CLIC_CLIC_CLICINT_PRIORITY_PRIOLEVEL3, /**< Priority level 3. */
    } vpr_clic_priority_t;
#else
    typedef uint8_t vpr_clic_priority_t;
#endif

int vevif_init(vpr_clic_priority_t interrupt_priority, vevif_event_handler_t event_handler, void *p_context);
void vevif_uninit(void);
void vevif_int_enable(uint32_t mask);
void vevif_int_disable(uint32_t mask);
void vevif_irq_handler(void);

#endif /* IPC_SERVICE_VEVIF_H */