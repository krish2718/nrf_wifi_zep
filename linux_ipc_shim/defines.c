#include "meos/ipc_service/defines.h"

struct device *getIpcInstance(const struct ipc_device_wrapper *ipc_dev)
{
    // Although we use one rx mbox instance to service many channels, the
    // vevif and bellboard are two completely different backends therefore they
    // cannot share the same rx mbox instance. So we pre-created and select the
    // appropriate instance based on the signal_type
    struct mbox_ids_conf *rx_conf = GET_IDS_CONFIG_RX(ipc_dev->signal_type);

    // RX: one IRQ to service multiple channels
    rx_conf->irq = IDS_CONFIG_GET_IRQ(ipc_dev->signal_type);
    rx_conf->instance = *ipc_dev->rx_instance;
    rx_conf->is_local  = true;

    ipc_dev->rx_device_mbox->config = rx_conf;

    // Assign to config backend which is defined by user in fw code.
    // This can only be done at run-time as it depends on the signal_type
    // being registered.
    struct icmsg_config_t *mbox_cfg = (struct icmsg_config_t *) ipc_dev->ipc->config;
    assert(mbox_cfg);
    mbox_cfg->mbox_tx.dev = ipc_dev->tx_device_mbox;
    mbox_cfg->mbox_rx.dev = ipc_dev->rx_device_mbox;

#ifdef CONFIG_IPC_SERVICE_PBUF
    /**
     * Since TX/RX addresses and sizes are user-defined within a structure in
     * the FW code, these are not known at compile time. Hence we can only
     * initialise a pbuf config at run-time.
     * @see `PBUF_DEFINE` and `PBUF_CFG_INIT` in pbuf.h
     */
    #define MEOS_PBUF_CFG_INIT(cfg, addr, size, dcache_align) do {                                              \
        (cfg)->rd_idx_loc = (uint32_t *) addr;                                                                  \
        (cfg)->wr_idx_loc = (uint32_t *) ((uint8_t *) addr + MAX(dcache_align, _PBUF_IDX_SIZE));                \
        (cfg)->data_loc = (uint8_t *) ((uint8_t *) addr + MAX(dcache_align, _PBUF_IDX_SIZE) + _PBUF_IDX_SIZE);  \
        (cfg)->len = (uint32_t)((uint32_t)(size) - MAX(dcache_align, _PBUF_IDX_SIZE) - _PBUF_IDX_SIZE);         \
        (cfg)->dcache_alignment = dcache_align;                                                                 \
    } while (0)

    #define MEOS_PBUF_DCACHE_ALIGN (0) /* Data cache line size (not used) */

    struct icmsg_data_t *backend_data = (struct icmsg_data_t *) ipc_dev->ipc->data;
    assert(backend_data);

    uint32_t tx_addr = mbox_cfg->tx_shm_addr;
    uint32_t tx_size = mbox_cfg->tx_shm_size;
    MEOS_PBUF_CFG_INIT(backend_data->tx_pb->cfg, tx_addr, tx_size, MEOS_PBUF_DCACHE_ALIGN);

    uint32_t rx_addr = mbox_cfg->rx_shm_addr;
    uint32_t rx_size = mbox_cfg->rx_shm_size;
    MEOS_PBUF_CFG_INIT(backend_data->rx_pb->cfg, rx_addr, rx_size, MEOS_PBUF_DCACHE_ALIGN);
#endif

    return ipc_dev->ipc;
}