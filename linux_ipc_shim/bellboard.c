#include "meos/ipc_service/bellboard.h"
#include "meos/ipc_service/nrf_common.h"
#include "meos/ipc_service/device.h"
#include "meos/kernel/krn.h"
#include "meos/irq/irq.h"

#include <assert.h>

enum {
    NRFX_BELLBOARD0_INST_IDX,
    NRFX_BELLBOARD1_INST_IDX,
#if defined(__UCC__) && __UCC__ == 710
    NRFX_BELLBOARD2_INST_IDX,
    NRFX_BELLBOARD3_INST_IDX,
#endif
    NRFX_BELLBOARD_ENABLED_COUNT
};

typedef struct
{
    bellboard_event_handler_t   handler;
    void *                      context;
    uint32_t                    int_pend;
    uint8_t                     int_idx;
    drv_state_t                 state;
} bellboard_cb_t;

static bellboard_cb_t m_cb[NRFX_BELLBOARD_ENABLED_COUNT];

int bellboard_init( nrfx_bellboard_t const *    p_instance,
                    uint8_t                     interrupt_priority,
                    bellboard_event_handler_t   event_handler,
                    void *                      p_context)
{
    assert(p_instance);

    bellboard_cb_t *p_cb = &m_cb[p_instance->drv_inst_idx];

    if (p_cb->state == DRV_STATE_INITIALIZED)
    {
        return -1;
    }

    // Intentionally empty. Priorities of IRQs are set through IRQ_route
    (void) interrupt_priority;

    p_cb->state     = DRV_STATE_INITIALIZED;
    p_cb->handler   = event_handler;
    p_cb->context   = p_context;
    p_cb->int_idx   = p_instance->int_idx;

    // Enable VPR realtime peripherals
    csr_write(CSR_NORDIC_CTRL,
              VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Max << VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Pos |
              VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Enabled << VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Pos);

    return 0;
}

void bellboard_uninit(nrfx_bellboard_t const *p_instance)
{
    bellboard_cb_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    assert(p_cb->state == DRV_STATE_INITIALIZED);

    p_cb->handler = NULL;
    p_cb->state = DRV_STATE_UNINITIALIZED;
}

void bellboard_int_enable(nrfx_bellboard_t const *p_instance, uint32_t mask)
{
    assert(p_instance);
    assert(m_cb[p_instance->drv_inst_idx].state == DRV_STATE_INITIALIZED);

    switch (p_instance->int_idx)
    {
#if defined(__UCC__) && __UCC__ == 710
        case 0:
            CLIC.CLICINT[BELLBOARD_WIFI_0_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET0 = mask;
            break;
        case 1:
            CLIC.CLICINT[BELLBOARD_WIFI_1_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET1 = mask;
            break;
        case 2:
            CLIC.CLICINT[BELLBOARD_WIFI_2_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET2 = mask;
            break;
        case 3:
            CLIC.CLICINT[BELLBOARD_WIFI_3_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET3 = mask;
            break;
#elif defined(__UCC__) && __UCC__ == 720
#ifdef NRF_LMAC
        case 0:
            CLIC.CLICINT[BELLBOARD_WIFI_0_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET0 = mask;
            break;
        case 1:
            CLIC.CLICINT[BELLBOARD_WIFI_1_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET1 = mask;
            break;
#else
        case 0:
            CLIC.CLICINT[BELLBOARD_WIFI_2_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET2 = mask;
            break;
        case 1:
            CLIC.CLICINT[BELLBOARD_WIFI_3_IRQn] |= CLIC_CLIC_CLICINT_IE_Msk;
            NRF_WIFICORE_BELLBOARD->INTENSET3 = mask;
            break;
#endif
#endif
        default:
            assert(0);
            break;
    }
}

void bellboard_int_disable(nrfx_bellboard_t const *p_instance, uint32_t mask)
{
    assert(p_instance);
    assert(m_cb[p_instance->drv_inst_idx].state == DRV_STATE_INITIALIZED);

    switch (p_instance->int_idx)
    {
#if defined(__UCC__) && __UCC__ == 710
        case 0:
            NRF_WIFICORE_BELLBOARD->INTENCLR0 = mask;
            break;
        case 1:
            NRF_WIFICORE_BELLBOARD->INTENCLR1 = mask;
            break;
        case 2:
            NRF_WIFICORE_BELLBOARD->INTENCLR2 = mask;
            break;
        case 3:
            NRF_WIFICORE_BELLBOARD->INTENCLR3 = mask;
            break;
#elif defined(__UCC__) && __UCC__ == 720
#ifdef NRF_LMAC
        case 0:
            NRF_WIFICORE_BELLBOARD->INTENCLR0 = mask;
            break;
        case 1:
            NRF_WIFICORE_BELLBOARD->INTENCLR1 = mask;
            break;
#else
        case 0:
            NRF_WIFICORE_BELLBOARD->INTENCLR2 = mask;
            break;
        case 1:
            NRF_WIFICORE_BELLBOARD->INTENCLR3 = mask;
            break;
#endif
#endif
        default:
            assert(0);
            break;
    }
}

uint32_t bellboard_int_pending_get(NRF_BELLBOARD_Type const *p_reg, uint8_t group_idx)
{
    switch (group_idx)
    {
#if defined(__UCC__) && __UCC__ == 710
        case 0:
            return p_reg->INTPEND0;
        case 1:
            return p_reg->INTPEND1;
        case 2:
            return p_reg->INTPEND2;
        case 3:
            return p_reg->INTPEND3;
#elif defined(__UCC__) && __UCC__ == 720
#ifdef NRF_LMAC
        case 0:
            return p_reg->INTPEND0;
        case 1:
            return p_reg->INTPEND1;
#else
        case 0:
            return p_reg->INTPEND2;
        case 1:
            return p_reg->INTPEND3;
#endif
#endif
        default:
            assert(0);
            return 0;
    }
}

void bellboard_irq_handler(void)
{
    int32_t irq_idx = IRQ_getCurrentIntNum();
#if defined(NRF_UMAC) && defined(__UCC__) && __UCC__ == 720
    int32_t bellboardNum = irq_idx - BELLBOARD_WIFI_2_IRQn;
#else
    int32_t bellboardNum = irq_idx - BELLBOARD_WIFI_0_IRQn;
#endif

    assert(bellboardNum >= 0 && bellboardNum < NRFX_BELLBOARD_ENABLED_COUNT);

    bellboard_cb_t *p_cb = NULL;

    for (uint8_t i = 0; i < NRFX_BELLBOARD_ENABLED_COUNT; i++)
    {
        if (m_cb[i].state == DRV_STATE_INITIALIZED && m_cb[i].int_idx == bellboardNum)
        {
            p_cb = &m_cb[i];
            break;
        }
    }

    assert(p_cb);
    assert(p_cb->context);
    assert(p_cb->handler);
    assert(p_cb->int_idx == bellboardNum);

    uint32_t int_pend = bellboard_int_pending_get(NRF_WIFICORE_BELLBOARD, p_cb->int_idx);

    while (int_pend)
    {
        uint8_t event_no = __builtin_ctz(int_pend);
        NRF_WIFICORE_BELLBOARD->EVENTS_TRIGGERED[event_no] = 0;
        p_cb->handler(event_no, p_cb->context);
        bitmask_bit_clear(event_no, &int_pend);
    }
}
