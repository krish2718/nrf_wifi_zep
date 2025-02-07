#ifndef IPC_SERVICE_ICMSG_H
#define IPC_SERVICE_ICMSG_H

#include "ipc_service.h"
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/timer.h>

// TODO: Make this define configurable through Makefile
//#define CONFIG_IPC_SERVICE_ICMSG_NOCOPY_RX

enum icmsg_state {
    ICMSG_STATE_OFF,
    ICMSG_STATE_BUSY,
    ICMSG_STATE_READY,
};

struct icmsg_config_t {
    uintptr_t tx_shm_addr;
    uintptr_t rx_shm_addr;
    size_t tx_shm_size;
    size_t rx_shm_size;
    struct mbox_channel mbox_tx;
    struct mbox_channel mbox_rx;
};

struct icmsg_state_t {
    volatile int value;
    struct semaphore lock;
};

struct icmsg_data_t {
    /* Tx/Rx buffers. */
    struct spsc_pbuf *tx_ib;
    struct spsc_pbuf *rx_ib;

    struct icmsg_state_t tx_buffer_state;

    /* Callbacks for an endpoint. */
    const struct ipc_service_cb *cb;
    void *ctx;

    /* General */
    const struct icmsg_config_t *cfg;
    struct timer_list notify_timer;
    struct icmsg_state_t state;

    /* No-copy */
#ifdef CONFIG_IPC_SERVICE_ICMSG_NOCOPY_RX
    struct icmsg_state_t rx_buffer_state;
    const void *rx_buffer;
    uint16_t rx_len;
#endif
};


int icmsg_open(const struct icmsg_config_t *conf,
           struct icmsg_data_t *dev_data,
           const struct ipc_service_cb *cb, void *ctx);

int icmsg_send(const struct icmsg_config_t *conf,
           struct icmsg_data_t *dev_data,
           const void *msg, size_t len);

#endif /* IPC_SERVICE_ICMSG_H */