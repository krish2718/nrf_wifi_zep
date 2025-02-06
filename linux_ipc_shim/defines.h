#ifndef IPC_SERVICE_DEFINES_H
#define IPC_SERVICE_DEFINES_H

#include "meos/ipc_service/device.h"
#include "meos/ipc_service/icmsg.h"
#include "meos/ipc_service/mbox_ids.h"
#include "meos/ipc_service/ids.h"

#include <errno.h>
#include <stdint.h>

#ifdef CONFIG_IPC_SERVICE_PBUF
    #include "meos/ipc_service/pbuf.h"
#endif

/* Size of PBUF read buffer in bytes */
#define CONFIG_PBUF_RX_READ_BUF_SIZE 128
/* Length of single IPC message in bytes */
#define CONFIG_APP_IPC_SERVICE_MESSAGE_LEN 100

/*
 * Notes from NCS.
 *
 * ~~~~~IRQ handlers ~~~~~
 *
 * VEVIF and BELLBOARDs are two different beasts that must be tamed by the same
 * driver.
 *
 * For BELLBOARDs we have 4 interrupt lines, one for each instance. Each
 * interrupt line / instance is managing several channels.
 *
 * So for example:
 *
 *   bellboard_application: bellboard@9a000 {
 *        compatible = "nordic,mbox-nrf-ids";
 *        reg = <0x9a000 0x1000>;
 *        interrupts = <96 NRF_DEFAULT_IRQ_PRIORITY>;
 *        instance = <1>;
 *        status = "okay";
 *        #mbox-cells = <1>;
 *   };
 *
 * In this case we have the BELLBOARD instance 1, that is using the IRQ line
 * #96 to service several inbound channels.
 *
 * The ISR servicing this line will be 'nrfx_ids_1_irq_handler' as defined into
 * the NRFX.
 *
 * VEVIF on the other hand is different. VEVIF hardware is exposing 32 IRQ
 * lines, each one servicing a single inbound channel.
 *
 * In the DT for VEVIF we have (for example):
 *
 *   vevif: vevif@8a000 {
 *        compatible = "nordic,mbox-nrf-ids";
 *        reg = <0x9a000 0x1000>;
 *        interrupts = <10>, <11>, <12>, ...;
 *        instance = <0>;
 *        status = "okay";
 *        #mbox-cells = <1>;
 *   };
 *
 * In this case the driver is realizing that we have multiple interrupts for
 * the same device (this only happens for VEVIF), so it is installing several
 * ISRs at the same time. In particular:
 *
 * nrfx_ids_0_irq_handler on IRQ line #10
 * nrfx_ids_1_irq_handler on IRQ line #11
 * nrfx_idx_2_irq_handler on IRQ line #12
 * ...
 *
 */

#define _CONCAT(x, y) x##y
#define _CONCAT3(x, y, z) x##y##z
#define STRINGIFY(x) #x

/**
 * Get the IRQ index from a given signal type (internal use only)
 * - Use default IRQ 96 for bellboard
 * - Unused for vevif
 */
#if defined(NRF_UMAC) && defined(__UCC__) && __UCC__ == 720
#define IDS_CONFIG_GET_IRQ(signal_type) (   \
    (signal_type) == IDS_BELLBOARD_SIGNAL   \
        ? BELLBOARD_WIFI_2_IRQn             \
        : 0                                 \
)
#else
#define IDS_CONFIG_GET_IRQ(signal_type) (   \
    (signal_type) == IDS_BELLBOARD_SIGNAL   \
        ? BELLBOARD_WIFI_0_IRQn             \
        : 0                                 \
)
#endif

#define CREATE_IDS_INSTANCE(id, signal_type)                \
{                                                           \
    .drv_inst_idx   = _CONCAT3(NRFX_IDS, id, _INST_IDX),    \
    .int_idx        = id,                                   \
    .sig_inst_type  = signal_type,                          \
}

__attribute__((__unused__)) static struct mbox_ids_conf rx_ids_config_bellboard;
__attribute__((__unused__)) static struct mbox_ids_conf rx_ids_config_vevif;

__attribute__((__unused__)) static struct mbox_ids_data rx_mbox_data_bellboard;
__attribute__((__unused__)) static struct mbox_ids_data rx_mbox_data_vevif;

#define GET_IDS_CONFIG_RX(signal_type) (    \
    (signal_type) == IDS_BELLBOARD_SIGNAL   \
        ? &rx_ids_config_bellboard          \
        : &rx_ids_config_vevif              \
)

#define GET_MBOX_DATA_RX(signal_type) (     \
    (signal_type) == IDS_BELLBOARD_SIGNAL   \
        ? &rx_mbox_data_bellboard           \
        : &rx_mbox_data_vevif               \
)

extern const struct mbox_driver_api ids_driver_api;
extern const struct ipc_service_backend backend_ops;

struct ipc_device_wrapper {
    struct device *tx_device_mbox;
    struct device *rx_device_mbox;
    struct device *ipc;
    uint8_t signal_type;
    nrfx_ids_t *rx_instance;
};

#ifdef CONFIG_IPC_SERVICE_PBUF
    /**
     * Create and prepare a pbuf config for run-time initialisation.
     * @see `PBUF_DEFINE` in pbuf.h
     */
    #define DEFINE_BACKEND_DATA(name)                   \
                                                        \
        static struct pbuf_cfg cfg_tx_pb_##name;        \
        static struct pbuf tx_pb_##name = {             \
            .cfg = &cfg_tx_pb_##name,                   \
        };                                              \
                                                        \
        static struct pbuf_cfg cfg_rx_pb_##name;        \
        static struct pbuf rx_pb_##name = {             \
            .cfg = &cfg_rx_pb_##name,                   \
        };                                              \
                                                        \
        static struct icmsg_data_t name = {             \
            .tx_pb = &tx_pb_##name,                     \
            .rx_pb = &rx_pb_##name                      \
        }
#else
    #define DEFINE_BACKEND_DATA(name)                   \
        static struct icmsg_data_t name
#endif

/**
 * Compile-time assertion to ensure a variable is not const-qualified.
 *
 * This macro creates a function that attempts to assign the variable to itself
 * If the variable is const-qualified, this assigment will fail at compile-time,
 * generating a compiler error.
 */
#define ASSERT_NOT_CONST(var) \
    void __assert_not_const_##var(void) { \
        var = var; /* This will fail if var is const */ \
    }

/**
 * Populate compile-time structures for IPC service
 * These are needed because IPC backend (icmsg) expects these
 */
#define CREATE_IPC_SERVICE(ipc_name, backend_cfg, tx_domain_id, rx_signal_type)         \
    /* backend_cfg must not be a constant */                                            \
    ASSERT_NOT_CONST(backend_cfg);                                                      \
                                                                                        \
    DEFINE_BACKEND_DATA(backend_data_##ipc_name);                                       \
                                                                                        \
    /* Always use instance #0 which means one irq will service many channels */         \
    static nrfx_ids_t rx_instance_##ipc_name = CREATE_IDS_INSTANCE(0, rx_signal_type);  \
                                                                                        \
    static struct mbox_ids_conf tx_ids_config_##ipc_name = {                            \
        .domain = tx_domain_id,                                                         \
        .is_local  = true,                                                              \
    };                                                                                  \
    static struct device tx_device_mbox_##ipc_name = {                                  \
        .name = STRINGIFY(tx_device_mbox_##ipc_name),                                   \
        .data = NULL,                                                                   \
        .api = &ids_driver_api,                                                         \
        .config = &tx_ids_config_##ipc_name,                                            \
    };                                                                                  \
    static struct device rx_device_mbox_##ipc_name = {                                  \
        .name = STRINGIFY(rx_device_mbox_##ipc_name),                                   \
        .data = GET_MBOX_DATA_RX(rx_signal_type),                                       \
        .api = &ids_driver_api,                                                         \
    };                                                                                  \
    static struct device device_##ipc_name = {                                          \
        .name = STRINGIFY(ipc_name),                                                    \
        .data = &backend_data_##ipc_name,                                               \
        .config = &backend_cfg,                                                         \
        .api = &backend_ops,                                                            \
    };                                                                                  \
    const struct ipc_device_wrapper ipc_name = {                                        \
        .tx_device_mbox = &tx_device_mbox_##ipc_name,                                   \
        .rx_device_mbox = &rx_device_mbox_##ipc_name,                                   \
        .ipc = &device_##ipc_name,                                                      \
        .signal_type = rx_signal_type,                                                  \
        .rx_instance = &rx_instance_##ipc_name                                          \
    };

struct device *getIpcInstance(const struct ipc_device_wrapper *ipc_dev);

#define GET_IPC_INSTANCE(ipc_dev) \
    getIpcInstance(ipc_dev);

#endif /* IPC_SERVICE_DEFINES_H */
