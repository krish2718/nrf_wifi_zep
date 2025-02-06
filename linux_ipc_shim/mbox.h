#ifndef IPC_SERVICE_MBOX_H
#define IPC_SERVICE_MBOX_H

#include "meos/ipc_service/device.h"

#include <errno.h>
#include <stdio.h>
#include <assert.h>

struct mbox_msg {
    /** Pointer to the data sent in the message. */
    const void *data;

    /** Size of the data. */
    size_t size;
};

struct mbox_channel {
    /** MBOX device pointer. */
    const struct device *dev;

    /** Channel ID. */
    uint32_t id;
};

typedef void (*mbox_callback_t)(const struct device *dev, uint32_t channel, void *user_data, struct mbox_msg *data);
typedef int (*mbox_send_t)(const struct device *dev, uint32_t channel, const struct mbox_msg *msg);
typedef int (*mbox_mtu_get_t)(const struct device *dev);
typedef int (*mbox_register_callback_t)(const struct device *dev, uint32_t channel, mbox_callback_t cb,	void *user_data);
typedef int (*mbox_set_enabled_t)(const struct device *dev, uint32_t channel, bool enable);
typedef uint32_t (*mbox_max_channels_get_t)(const struct device *dev);

struct mbox_driver_api {
    mbox_send_t send;
    mbox_register_callback_t register_callback;
    // mbox_mtu_get_t mtu_get;
    // mbox_max_channels_get_t max_channels_get;
    mbox_set_enabled_t set_enabled;
};

static inline void mbox_init_channel(struct mbox_channel *channel, const struct device *dev, uint32_t ch_id)
{
    channel->dev = dev;
    channel->id = ch_id;
}

static inline int mbox_send(const struct mbox_channel *channel, const struct mbox_msg *msg)
{
    const struct mbox_driver_api *api = (const struct mbox_driver_api *) ((uintptr_t)channel->dev->api);

    assert(api != NULL);
    assert(api->send != NULL);
    return api->send(channel->dev, channel->id, msg);
}

static inline int mbox_register_callback(const struct mbox_channel *channel, mbox_callback_t cb, void *user_data)
{
    const struct mbox_driver_api *api = (const struct mbox_driver_api *) channel->dev->api;

    if (api->register_callback == NULL) {
        return -ENOSYS;
    }

    return api->register_callback(channel->dev, channel->id, cb, user_data);
}

static inline int mbox_set_enabled(const struct mbox_channel *channel, bool enable)
{
    const struct mbox_driver_api *api = (const struct mbox_driver_api *) channel->dev->api;

    assert(api->set_enabled != NULL);
    return api->set_enabled(channel->dev, channel->id, enable);
}

#endif /* IPC_SERVICE_MBOX_H */
