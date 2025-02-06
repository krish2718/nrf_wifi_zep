#ifndef IPC_SERVICE_MBOX_IDS_H
#define IPC_SERVICE_MBOX_IDS_H

#include "meos/ipc_service/ids.h"
#include "meos/ipc_service/mbox.h"

#include "wifi_mdk_common.h"

struct mbox_ids_conf {
    nrfx_ids_t instance;
    nrfx_ids_domain_t domain;
    unsigned int irq;
    bool is_local;
};

struct mbox_ids_data {
    mbox_callback_t cb[NRFX_IDS_EVENTS_TRIGGERED_COUNT];
    void *user_data[NRFX_IDS_EVENTS_TRIGGERED_COUNT];
    const struct device *dev;
    uint32_t enabled_mask;
};

#endif /* IPC_SERVICE_MBOX_IDS_H */
