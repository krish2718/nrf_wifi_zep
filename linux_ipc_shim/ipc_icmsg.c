#include "meos/ipc_service/ipc_icmsg.h"
#include "meos/ipc_service/icmsg.h"
#include "meos/ipc_service/ipc_service_backend.h"

static int register_ept(const struct device *instance, void **token,
            const struct ipc_ept_cfg *cfg)
{
    const struct icmsg_config_t *conf = (const struct icmsg_config_t *) instance->config;
    struct icmsg_data_t *dev_data = (struct icmsg_data_t *) instance->data;

    /* Only one endpoint is supported. No need for a token. */
    *token = NULL;

    return icmsg_open(conf, dev_data, &cfg->cb, cfg->priv);
}

static int send(const struct device *instance, void *token, const void *msg, size_t len)
{
    (void) token;

    const struct icmsg_config_t *conf = (const struct icmsg_config_t *) instance->config;
    struct icmsg_data_t *dev_data = (struct icmsg_data_t *) instance->data;

    return icmsg_send(conf, dev_data, msg, len);
}

const struct ipc_service_backend backend_ops = {
    .register_endpoint = register_ept,
    .deregister_endpoint = NULL,
    .send = send,
};