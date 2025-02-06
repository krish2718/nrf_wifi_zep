#ifndef IPC_SERVICE_IPC_SERVICE_H
#define IPC_SERVICE_IPC_SERVICE_H

#include "meos/ipc_service/defines.h"
#include "meos/ipc_service/device.h"

struct ipc_service_cb {
    void (*bound)(void *priv);
    void (*received)(const void *data, size_t len, void *priv);
    void (*error)(const char *message, void *priv);
};

struct ipc_ept {
    const struct device *instance;
    void *token;
};

struct ipc_ept_cfg {
    const char *name;
    int prio;
    struct ipc_service_cb cb;
    void *priv;
};

int ipc_service_open_instance(const struct device *instance);

int ipc_service_close_instance(const struct device *instance);

int ipc_service_register_endpoint(const struct device *instance,
                    struct ipc_ept *ept,
                    const struct ipc_ept_cfg *cfg);

int ipc_service_send(struct ipc_ept *ept, const void *data, size_t len);

#endif /* IPC_SERVICE_IPC_SERVICE_H */