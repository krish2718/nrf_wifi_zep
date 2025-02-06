#ifndef IPC_SERVICE_DEVICE_H
#define IPC_SERVICE_DEVICE_H

#include <linux/stddef.h>
#include <linux/string.h>

struct device_state
{
    uint8_t init_res;
    bool initialized : 1;
};

struct device
{
    const char *name;
    const void *config;
    const void *api;
    struct device_state *state;
    const void *data;
};

#endif /* IPC_SERVICE_DEVICE_H */