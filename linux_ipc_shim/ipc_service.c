#include "meos/ipc_service/ipc_service.h"
#include "meos/ipc_service/ipc_service_backend.h"

#include <errno.h>
#include <stdio.h>
#include <assert.h>


int ipc_service_open_instance(const struct device *instance)
{
    (void) instance;
    return 0;
}

int ipc_service_close_instance(const struct device *instance)
{
    (void) instance;
    return 0;
}

int ipc_service_register_endpoint(const struct device *instance,
                    struct ipc_ept *ept,
                    const struct ipc_ept_cfg *cfg)
{
    assert(instance);

    const struct ipc_service_backend *backend;
    backend = (const struct ipc_service_backend *) instance->api;

    ept->instance = instance;

    return backend->register_endpoint(instance, &ept->token, cfg);
}

int ipc_service_send(struct ipc_ept *ept, const void *data, size_t len)
{
    const struct ipc_service_backend *backend;

    if (!ept)
    {
        printf("Invalid endpoint\n");
        return -EINVAL;
    }

    if (!ept->instance)
    {
        printf("Endpoint not registered\n");
        return -ENOENT;
    }

    backend = ept->instance->api;

    assert(backend != NULL);
    assert(backend->send != NULL);

    if (!backend || !backend->send)
    {
        printf("Invalid backend configuration\n");
        return -EIO;
    }

    return backend->send(ept->instance, ept->token, data, len);
}