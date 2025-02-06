#include "meos/ipc_service/icmsg.h"
#include "meos/kernel/krn.h"
#include "meos/irq/irq.h"

#include "wifi_mdk_common.h"

#include <errno.h>
#include <string.h>
#include <assert.h>

#ifdef CONFIG_IPC_SERVICE_PBUF
    #include "meos/ipc_service/pbuf.h"
#else
    #include "meos/ipc_service/spsc_pbuf.h"
#endif

enum rx_buffer_state {
    RX_BUFFER_STATE_RELEASED,
    RX_BUFFER_STATE_RELEASING,
    RX_BUFFER_STATE_HELD
};

enum tx_buffer_state {
    TX_BUFFER_STATE_UNUSED,
    TX_BUFFER_STATE_RESERVED
};

static const uint8_t magic[] = {
    0x45, 0x6d, 0x31, 0x6c, 0x31, 0x4b,
    0x30, 0x72, 0x6e, 0x33, 0x6c, 0x69, 0x34
};

static void notify_process(KRN_TIMER_T *timer, void *timerPar);
static void mbox_callback_process(struct icmsg_data_t *dev_data);
static int reserve_tx_buffer_if_unused(struct icmsg_data_t *dev_data);
static bool is_rx_buffer_free(struct icmsg_data_t *dev_data);
static bool is_rx_data_available(struct icmsg_data_t *dev_data);
static void submit_mbox_work(struct icmsg_data_t *dev_data);
static void submit_work_if_buffer_free(struct icmsg_data_t *dev_data);
static void submit_work_if_buffer_free_and_data_available(struct icmsg_data_t *dev_data);
static void mbox_callback(const struct device *instance, uint32_t channel, void *user_data, struct mbox_msg *msg_data);
static int mbox_init(const struct icmsg_config_t *conf, struct icmsg_data_t *dev_data);

#ifndef CONFIG_IPC_SERVICE_PBUF
    static bool is_rx_buffer_held(struct icmsg_data_t *dev_data);
#endif

static void atomic_init(struct icmsg_state_t *obj)
{
    KRN_initSemaphore(&obj->lock, 1);
}

static void atomic_store(struct icmsg_state_t *obj, int desired)
{
    KRN_testSemaphore(&obj->lock, 1, KRN_INFWAIT);
    obj->value = desired;
    KRN_setSemaphore(&obj->lock, 1);
}

static int atomic_load(struct icmsg_state_t *obj)
{
    KRN_testSemaphore(&obj->lock, 1, KRN_INFWAIT);
    int result = obj->value;
    KRN_setSemaphore(&obj->lock, 1);
    return result;
}

static void submit_work_if_buffer_free_and_data_available(
        struct icmsg_data_t *dev_data);

static bool is_endpoint_ready(struct icmsg_data_t *dev_data)
{
    return atomic_load(&dev_data->state) == ICMSG_STATE_READY;
}

static void notify_process(KRN_TIMER_T *timer, void *timerPar)
{
    struct icmsg_data_t *dev_data = (struct icmsg_data_t *) timerPar;
    assert(dev_data);

    mbox_send(&dev_data->cfg->mbox_tx, NULL);

    int state = atomic_load(&dev_data->state);

    if (state != ICMSG_STATE_READY)
    {
        /* Notification is repeated after 1 ms */
        KRN_setTimer(timer, (KRN_TIMERFUNC_T *) notify_process, dev_data, 1000);
    }
}

static void mbox_callback_process(struct icmsg_data_t *dev_data)
{
#ifdef CONFIG_IPC_SERVICE_PBUF
    uint8_t rx_buffer[CONFIG_PBUF_RX_READ_BUF_SIZE] __aligned(4);
#else
    char *rx_buffer;
#endif

    assert(dev_data);
    int state = atomic_load(&dev_data->state);

#ifdef CONFIG_IPC_SERVICE_PBUF
    uint32_t len = is_rx_data_available(dev_data);
#else
    uint16_t len = spsc_pbuf_claim(dev_data->rx_ib, &rx_buffer);
#endif

    if (len == 0)
    {
        /* Unlikely, no data in buffer */
        return;
    }

#ifdef CONFIG_IPC_SERVICE_PBUF
    len = pbuf_read(dev_data->rx_pb, (char *) rx_buffer, sizeof(rx_buffer));
#endif

    if (state == ICMSG_STATE_READY)
    {
        if (dev_data->cb->received)
        {
            dev_data->cb->received(rx_buffer, len, dev_data->ctx);

#ifndef CONFIG_IPC_SERVICE_PBUF
            /* Release Rx buffer here only in case when user did not request
            * to hold it.
            */
            if (!is_rx_buffer_held(dev_data))
            {
                spsc_pbuf_free(dev_data->rx_ib, len);
            }
#endif
        }
    }
    else
    {
        assert(state == ICMSG_STATE_BUSY);

        bool endpoint_invalid = (len != sizeof(magic) || memcmp(magic, rx_buffer, len));

#ifndef CONFIG_IPC_SERVICE_PBUF
        spsc_pbuf_free(dev_data->rx_ib, len);
#endif

        if (endpoint_invalid)
        {
            assert(0);
            return;
        }

        if (dev_data->cb->bound)
        {
            dev_data->cb->bound(dev_data->ctx);
        }

        atomic_store(&dev_data->state, ICMSG_STATE_READY);
    }

    submit_work_if_buffer_free_and_data_available(dev_data);
}

static int reserve_tx_buffer_if_unused(struct icmsg_data_t *dev_data)
{
    bool was_unused;

    if (atomic_load(&dev_data->tx_buffer_state) == TX_BUFFER_STATE_UNUSED)
    {
        atomic_store(&dev_data->tx_buffer_state, TX_BUFFER_STATE_RESERVED);
        was_unused = true;
    }
    else
    {
        was_unused = false;
    }

    return was_unused ? 0 : -EALREADY;
}

static int release_tx_buffer(struct icmsg_data_t *dev_data)
{
    bool was_reserved;

    if (atomic_load(&dev_data->tx_buffer_state) == TX_BUFFER_STATE_RESERVED)
    {
        atomic_store(&dev_data->tx_buffer_state, TX_BUFFER_STATE_UNUSED);
        was_reserved = true;
    }
    else
    {
        was_reserved = false;
    }

    return was_reserved ? 0 : -EALREADY;
}

static bool is_rx_buffer_free(struct icmsg_data_t *dev_data)
{
#ifdef CONFIG_IPC_SERVICE_ICMSG_NOCOPY_RX
    return atomic_load(&dev_data->rx_buffer_state) == RX_BUFFER_STATE_RELEASED;
#else
    (void) dev_data;
    return true;
#endif
}

#ifndef CONFIG_IPC_SERVICE_PBUF
static bool is_rx_buffer_held(struct icmsg_data_t *dev_data)
{
#ifdef CONFIG_IPC_SERVICE_ICMSG_NOCOPY_RX
    return atomic_load(&dev_data->rx_buffer_state) == RX_BUFFER_STATE_HELD;
#else
    (void) dev_data;
    return false;
#endif
}
#endif

static bool is_rx_data_available(struct icmsg_data_t *dev_data)
{
    int len;

#ifdef CONFIG_IPC_SERVICE_PBUF
    len = pbuf_read(dev_data->rx_pb, NULL, 0);
#else
    len = spsc_pbuf_read(dev_data->rx_ib, NULL, 0);
#endif

    return len > 0;
}

static void submit_mbox_work(struct icmsg_data_t *dev_data)
{
    mbox_callback_process(dev_data);
}

static void submit_work_if_buffer_free(struct icmsg_data_t *dev_data)
{
    if (!is_rx_buffer_free(dev_data))
    {
        assert(0);
        return;
    }

    submit_mbox_work(dev_data);
}

static void submit_work_if_buffer_free_and_data_available(struct icmsg_data_t *dev_data)
{
    if (!is_rx_buffer_free(dev_data))
    {
        return;
    }

    if (!is_rx_data_available(dev_data))
    {
        return;
    }

    submit_mbox_work(dev_data);
}

static void mbox_callback(const struct device *instance, uint32_t channel, void *user_data, struct mbox_msg *msg_data)
{
    (void) instance;
    (void) channel;
    (void) msg_data;

    struct icmsg_data_t *dev_data = user_data;
    submit_work_if_buffer_free(dev_data);
}

static int mbox_init(const struct icmsg_config_t *conf, struct icmsg_data_t *dev_data)
{
    int err = mbox_register_callback(&conf->mbox_rx, mbox_callback, dev_data);

    if (err != 0)
    {
        return err;
    }

    return mbox_set_enabled(&conf->mbox_rx, 1);
}

int icmsg_open(const struct icmsg_config_t *conf,
           struct icmsg_data_t *dev_data,
           const struct ipc_service_cb *cb, void *ctx)
{
    int ret;

    /* Initialise atomic states */
    atomic_init(&dev_data->tx_buffer_state);
    atomic_init(&dev_data->state);
#ifdef CONFIG_IPC_SERVICE_ICMSG_NOCOPY_RX
    atomic_init(&dev_data->rx_buffer_state);
#endif

    if (atomic_load(&dev_data->state) == ICMSG_STATE_OFF)
    {
        atomic_store(&dev_data->state, ICMSG_STATE_BUSY);
    }
    else
    {
        /* Already opened. */
        return -EALREADY;
    }

    dev_data->cb = cb;
    dev_data->ctx = ctx;
    dev_data->cfg = conf;

#ifdef CONFIG_IPC_SERVICE_PBUF
    ret = pbuf_tx_init(dev_data->tx_pb);

    if (ret < 0) {
        assert(0);
        return ret;
    }

    (void)pbuf_rx_init(dev_data->rx_pb);
    ret = pbuf_write(dev_data->tx_pb, (const char *) magic, sizeof(magic));
#else
    assert(conf->tx_shm_size > sizeof(struct spsc_pbuf));

    dev_data->tx_ib = spsc_pbuf_init((void *)conf->tx_shm_addr,
                        conf->tx_shm_size,
                        0);
    dev_data->rx_ib = (void *) conf->rx_shm_addr;

    ret = spsc_pbuf_write(dev_data->tx_ib, (const char *) magic, sizeof(magic));
#endif

    if (ret < 0)
    {
        __ASSERT_NO_MSG(false);
        return ret;
    }

    if (ret < (int)sizeof(magic))
    {
        __ASSERT_NO_MSG(ret == sizeof(magic));
        return ret;
    }

    ret = mbox_init(conf, dev_data);
    if (ret)
    {
        return ret;
    }

    KRN_setTimer(&dev_data->notify_timer, (KRN_TIMERFUNC_T *) notify_process, dev_data, 10);
    return ret;
}

int icmsg_send(const struct icmsg_config_t *conf, struct icmsg_data_t *dev_data, const void *msg, size_t len)
{
    int ret;
    int write_ret;
    __attribute__((unused)) int release_ret;
    int sent_bytes;

    if (!is_endpoint_ready(dev_data))
    {
        return -EBUSY;
    }

    /* Empty message is not allowed */
    if (len == 0)
    {
        return -ENODATA;
    }

    ret = reserve_tx_buffer_if_unused(dev_data);
    if (ret)
    {
        return -ENOBUFS;
    }

#ifdef CONFIG_IPC_SERVICE_PBUF
    write_ret = pbuf_write(dev_data->tx_pb, msg, len);
#else
    write_ret = spsc_pbuf_write(dev_data->tx_ib, msg, len);
#endif

    release_ret = release_tx_buffer(dev_data);
    assert(!release_ret);

    if (write_ret < 0)
    {
        return write_ret;
    }
    else if (write_ret < (int) len)
    {
        return -EBADMSG;
    }
    sent_bytes = write_ret;

    assert(conf->mbox_tx.dev != NULL);

    ret = mbox_send(&conf->mbox_tx, NULL);
    if (ret)
    {
        return ret;
    }

    return sent_bytes;
}
