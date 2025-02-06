#ifndef IPC_SERVICE_IDS_H
#define IPC_SERVICE_IDS_H

#if defined(__UCC__) && __UCC__ == 720
#include "nrf7120_types.h"
#include "nrf7120_application.h"
#else
#include "nrf7140_types.h"
#include "nrf7140_application.h"
#endif

#include "meos/ipc_service/vevif.h"
#include "meos/ipc_service/bellboard.h"

#include <stdio.h>
#include <assert.h>

/**
 * The number of events trigger count.
 * For bellboard and vevif, this is the EVENTS_TRIGGERED_MaxCount which has the
 * the same number.
*/
#define NRFX_IDS_EVENTS_TRIGGERED_COUNT 32

typedef enum {
    IDS_BELLBOARD_SIGNAL,
    IDS_VEVIF_SIGNAL
} ids_signalling_device_t;

/**
 * Structure of the IDS instance
*/
typedef struct
{
    uint8_t drv_inst_idx; ///< Index of the instance. For internal use only.
    uint8_t int_idx;      ///< Interrupt index. For internal use only.

    /**
     * NOTE: Additional field to indicate the type of signalling this IDS belongs to.
     * Here is where we begin to divert from ncs-next. Over there, signalling mechanism
     * is hardcoded for specific processor; for example - RISC-V always handle
     * signals from ARM via vevif and signals via bellboard. WiFi is a special case
     * where LMAC is a RISC-V but it can receive signal of both bellboard and vevif
    */
    ids_signalling_device_t sig_inst_type;
} nrfx_ids_t;

/**
 * IDS instance index (internal use for bellboard only).
*/
enum {
    NRFX_IDS0_INST_IDX,
    NRFX_IDS1_INST_IDX,
    NRFX_IDS2_INST_IDX,
    NRFX_IDS3_INST_IDX,
    NRFX_IDS_ENABLED_COUNT
};

typedef enum
{
#if defined(__UCC__) && __UCC__ == 720
    NRFX_IDS_DOMAIN_APP      = NRF_PROCESSOR_CM33,
    NRFX_IDS_DOMAIN_WIFILMAC = NRF_PROCESSOR_LMAC,
    NRFX_IDS_DOMAIN_WIFIUMAC = NRF_PROCESSOR_UMAC,
#else
    NRFX_IDS_DOMAIN_SEC     = NRF_PROCESSOR_SECURE,      ///< Secure domain. */
    NRFX_IDS_DOMAIN_APP     = NRF_PROCESSOR_APPLICATION, ///< Application domain. */
    NRFX_IDS_DOMAIN_NET     = NRF_PROCESSOR_RADIOCORE,   ///< Network domain. */
    NRFX_IDS_DOMAIN_SYSCTRL = NRF_PROCESSOR_SYSCTRL,     ///< System Controller domain. */
    NRFX_IDS_DOMAIN_PPR     = NRF_PROCESSOR_PPR,         ///< Peripheral Processor */
    NRFX_IDS_DOMAIN_FLPR    = NRF_PROCESSOR_FLPR,        ///< Fast Lightweight Processor */

    /**
     * TODO: Only one domain for both LMAC and UMAC -- to be cleaned up
     *
     * Domain enumerations are used to indicate which processor to signal to.
     * Since there are dedicated vevif registers for both LMAC and UMAC they
     * would also need a dedicated domain ID ?
    */
    NRFX_IDS_DOMAIN_WIFILMAC    = NRF_PROCESSOR_WIFILMAC,
    NRFX_IDS_DOMAIN_WIFIUMAC    = NRF_PROCESSOR_WIFIUMAC,
#endif
} nrfx_ids_domain_t;

typedef void (*nrfx_ids_event_handler_t)(uint8_t event_idx, void *p_context);

static inline int ids_init( nrfx_ids_t const *          p_instance,
                            uint8_t                     interrupt_priority,
                            nrfx_ids_event_handler_t    event_handler,
                            void *                      p_context,
                            void const *                p_config)
{
    (void) p_config;

    switch (p_instance->sig_inst_type)
    {
        case IDS_BELLBOARD_SIGNAL:
            return bellboard_init(
                        (nrfx_bellboard_t const *) p_instance,
                        interrupt_priority,
                        (bellboard_event_handler_t) event_handler,
                        p_context);
        case IDS_VEVIF_SIGNAL:
            return vevif_init((vpr_clic_priority_t) interrupt_priority,
                        (vevif_event_handler_t) event_handler,
                        p_context);
        default:
            assert(0);
            return -1;
    }
}

static inline void ids_int_enable(nrfx_ids_t const *p_instance, uint32_t mask)
{
    switch (p_instance->sig_inst_type)
    {
        case IDS_BELLBOARD_SIGNAL:
            bellboard_int_enable((nrfx_bellboard_t const *) p_instance, mask);
            break;
        case IDS_VEVIF_SIGNAL:
            vevif_int_enable(mask);
            break;
        default:
            assert(0);
            break;
    }
}

static inline void ids_int_disable(nrfx_ids_t const *p_instance, uint32_t mask)
{
    switch (p_instance->sig_inst_type)
    {
        case IDS_BELLBOARD_SIGNAL:
            bellboard_int_disable((nrfx_bellboard_t const *) p_instance, mask);
            break;
        case IDS_VEVIF_SIGNAL:
            vevif_int_disable(mask);
            break;
        default:
            assert(0);
            break;
    }
}

static inline void ids_signal(nrfx_ids_t *p_instance, nrfx_ids_domain_t domain, uint8_t channel)
{
    assert(channel < NRFX_IDS_EVENTS_TRIGGERED_COUNT);

    (void) p_instance;
    NRF_BELLBOARD_Type * p_bell = NULL;
    NRF_VPR_Type       * p_vpr  = NULL;

    switch (domain)
    {
        case NRFX_IDS_DOMAIN_APP:
            p_bell = NRF_APPLICATION_BELLBOARD;
            break;

        // TODO: For now, signalling to wificore lmac and umac is via vevif only
        // Technically, through bellboard should also be possible.
        case NRFX_IDS_DOMAIN_WIFILMAC:
            p_vpr = NRF_WIFICORE_VPRLMAC;
            break;
        case NRFX_IDS_DOMAIN_WIFIUMAC:
            p_vpr = NRF_WIFICORE_VPRUMAC;
            break;

        default:
            assert(0);
            break;
    }

    if (p_bell)
    {
        p_bell->TASKS_TRIGGER[channel] = 1;
    }
    else
    {
        p_vpr->TASKS_TRIGGER[channel] = 1;
    }
}

#endif /* IPC_SERVICE_IDS_H */
