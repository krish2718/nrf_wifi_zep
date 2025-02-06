#include "meos/ipc_service/mbox_ids.h"
#include "meos/ipc_service/nrf_common.h"
#include "meos/ipc_service/vevif.h"
#include "meos/ipc_service/bellboard.h"
#include "meos/kernel/krn.h"
#include "meos/irq/irq.h"

#include <errno.h>
#include <stdio.h>

static inline bool is_rx_id_valid(struct mbox_ids_data *data, uint32_t id)
{
    return ((id < NRFX_IDS_EVENTS_TRIGGERED_COUNT) && (data->enabled_mask & BIT(id)));
}

static void ids_handler(uint8_t id, void *context)
{
    struct mbox_ids_data *data = (struct mbox_ids_data *) context;

    const struct device *dev = data->dev;
    const struct mbox_ids_conf *conf = dev->config;

    if (data == NULL)
    {
        fprintf(stderr, "RX event on illegal channel\n");
        return;
    }

    if (!is_rx_id_valid(data, id))
    {
        fprintf(stderr, "RX event on disabled channel, disable int: %d\n", id);
        ids_int_disable(&conf->instance, BIT(id));
        return;
    }

    if (data->cb[id] != NULL)
    {
        data->cb[id](dev, id, data->user_data[id], NULL);
    }
}

static int ids_init_instance(const struct device *dev, uint32_t id)
{
    const struct mbox_ids_conf *conf = dev->config;
    struct mbox_ids_data *data = (struct mbox_ids_data *) dev->data;

    data->dev = dev;

    IRQ_DESC_T descIrqDesc;

    switch (conf->instance.sig_inst_type)
    {
        case IDS_BELLBOARD_SIGNAL:
            descIrqDesc.intNum = conf->irq;
            descIrqDesc.isrFunc = bellboard_irq_handler;
            break;
        case IDS_VEVIF_SIGNAL:
            descIrqDesc.intNum = id;
            descIrqDesc.isrFunc = vevif_irq_handler;
            break;
        default:
            assert(0);
            break;
    }

    IRQ_route(&descIrqDesc);

    return ids_init(&conf->instance, 0, ids_handler, data, NULL);
}

static int ids_send(const struct device *dev, uint32_t id, const struct mbox_msg *msg)
{
    const struct mbox_ids_conf *conf = dev->config;

    if (id > NRFX_IDS_EVENTS_TRIGGERED_COUNT)
    {
        return -EINVAL;
    }

    if (msg)
    {
        printf("Sending data not supported\n");
    }

    ids_signal((nrfx_ids_t *) &conf->instance, conf->domain, id);
    return 0;
}


static int ids_register_callback(const struct device *dev, uint32_t id, mbox_callback_t cb, void *user_data)
{
    ids_init_instance(dev, id);

    const struct mbox_ids_conf *conf = dev->config;
    struct mbox_ids_data *data = (struct mbox_ids_data *) dev->data;

    if (!conf->is_local)
    {
        return -ENOSYS;
    }

    if (id >= NRFX_IDS_EVENTS_TRIGGERED_COUNT)
    {
        return -EINVAL;
    }

    data->cb[id] = cb;
    data->user_data[id] = user_data;

    return 0;
}

static int ids_set_enabled(const struct device *dev, uint32_t id, bool enable)
{
    const struct mbox_ids_conf *conf = dev->config;
    struct mbox_ids_data *data = (struct mbox_ids_data *) dev->data;

    if (id >= NRFX_IDS_EVENTS_TRIGGERED_COUNT)
    {
        return -EINVAL;
    }

    if ((enable == 0 && (!(data->enabled_mask & BIT(id)))) ||
        (enable != 0 && (data->enabled_mask & BIT(id))))
    {
        return -EALREADY;
    }

    if (enable)
    {
        data->enabled_mask |= (1 << id);
    }
    else
    {
        data->enabled_mask &= ~(1 << id);
    }

    ids_int_disable(&conf->instance, ~(data->enabled_mask));
    ids_int_enable(&conf->instance, data->enabled_mask);

    return 0;
}

const struct mbox_driver_api ids_driver_api = {
    .send = ids_send,
    .register_callback = ids_register_callback,
    .set_enabled = ids_set_enabled,
};