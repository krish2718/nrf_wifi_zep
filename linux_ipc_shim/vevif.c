#include "meos/ipc_service/vevif.h"
#include "meos/ipc_service/nrf_common.h"
#include "meos/ipc_service/device.h"
#include "meos/kernel/krn.h"
#include "meos/irq/irq.h"

#include <stdio.h>
#include <assert.h>

typedef struct
{
    vevif_event_handler_t   handler;
    void *                  p_context;
    drv_state_t             state;
} vevif_cb_t;

static vevif_cb_t m_cb;

int vevif_init(vpr_clic_priority_t      interrupt_priority,
                vevif_event_handler_t   event_handler,
                void *                  p_context)
{

    if (m_cb.state == DRV_STATE_INITIALIZED)
    {
        return -1;
    }

    // Intentionally empty. Priorities of IRQs are set through IRQ_route
    (void) interrupt_priority;

    m_cb.handler   = event_handler;
    m_cb.p_context = p_context;
    m_cb.state = DRV_STATE_INITIALIZED;

    csr_write(VPRCSR_MSTATUS, VPRCSR_MSTATUS_MIE_Msk);

    // Enable VPR realtime peripherals
    csr_write(CSR_NORDIC_CTRL,
              VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Max << VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Pos |
              VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Enabled << VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Pos);

    csr_write(VPRCSR_NORDIC_TASKS, 0);

    return 0;
}

void vevif_uninit(void)
{
    assert(m_cb.state == DRV_STATE_INITIALIZED);

    m_cb.handler = NULL;
    m_cb.state = DRV_STATE_UNINITIALIZED;
}

void vevif_int_enable(uint32_t mask)
{
    assert(m_cb.state == DRV_STATE_INITIALIZED);

    while (mask != 0)
    {
        uint32_t event_no = __builtin_ctz(mask);
        uint32_t irq_num = event_no;

        CLIC.CLICINT[irq_num] = (CLIC.CLICINT[irq_num] & ~CLIC_CLIC_CLICINT_IE_Msk) |
                                (CLIC_CLIC_CLICINT_IE_Enabled << CLIC_CLIC_CLICINT_IE_Pos);
        bitmask_bit_clear(event_no, (void *) &mask);
    }
}

void vevif_int_disable(uint32_t mask)
{
    assert(m_cb.state == DRV_STATE_INITIALIZED);

    while (mask != 0)
    {
        uint32_t event_no = __builtin_ctz(mask);
        uint32_t irq_num = event_no;

        CLIC.CLICINT[irq_num] = (CLIC.CLICINT[irq_num] & ~CLIC_CLIC_CLICINT_IE_Msk) |
                                (CLIC_CLIC_CLICINT_IE_Disabled << CLIC_CLIC_CLICINT_IE_Pos);
        bitmask_bit_clear(event_no, (void *) &mask);
    }
}

void vevif_irq_handler(void)
{
    int32_t irq_idx = IRQ_getCurrentIntNum();

    csr_clear_bits(VPRCSR_NORDIC_TASKS, 1 << irq_idx);
    m_cb.handler(irq_idx, m_cb.p_context);
}
