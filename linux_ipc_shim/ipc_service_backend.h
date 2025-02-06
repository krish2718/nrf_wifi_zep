#ifndef IPC_SERVICE_IPC_SERVICE_BACKEND_H
#define IPC_SERVICE_IPC_SERVICE_BACKEND_H

#include "ipc_service.h"

struct ipc_service_backend
{
    int (*open_instance)(const struct device *instance);
    int (*close_instance)(const struct device *instance);
    int (*send)(const struct device *instance, void *token,
            const void *data, size_t len);
    int (*register_endpoint)(const struct device *instance,
            void **token,
            const struct ipc_ept_cfg *cfg);
    int (*deregister_endpoint)(const struct device *instance, void *token);
};

#endif /* IPC_SERVICE_IPC_SERVICE_BACKEND_H */