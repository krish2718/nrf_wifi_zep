#include "defines.h"

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


    return ipc_dev->ipc;
}
