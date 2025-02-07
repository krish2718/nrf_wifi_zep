#ifndef IPC_SERVICE_DEFINES_H
#define IPC_SERVICE_DEFINES_H

#include <linux/kernel.h>
#include <linux/errno.h>

#include "device.h"
#include "icmsg.h"

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

extern const struct ipc_service_backend backend_ops;

struct ipc_device_wrapper {
    struct device *ipc;
};

#define DEFINE_BACKEND_DATA(name) static struct icmsg_data_t name

/**
 * Compile-time BUG_ONion to ensure a variable is not const-qualified.
 *
 * This macro creates a function that attempts to assign the variable to itself
 * If the variable is const-qualified, this assigment will fail at compile-time,
 * generating a compiler error.
 */
#define ASSERT_NOT_CONST(var) \
    do { \
        extern void __assert_not_const_##var(void); \
        if (0) __assert_not_const_##var(); \
    } while (0)

/**
 * Populate compile-time structures for IPC service
 * These are needed because IPC backend (icmsg) expects these
 */
#define CREATE_IPC_SERVICE(ipc_name, backend_cfg, tx_domain_id, rx_signal_type)         \
    /* backend_cfg must not be a constant */                                            \
    ASSERT_NOT_CONST(backend_cfg);                                                      \
                                                                                        \
    DEFINE_BACKEND_DATA(backend_data_##ipc_name);                                       \
                                                                                        \                                                                               \
    static struct device device_##ipc_name = {                                          \
        .name = STRINGIFY(ipc_name),                                                    \
        .data = &backend_data_##ipc_name,                                               \
        .config = &backend_cfg,                                                         \
        .api = &backend_ops,                                                            \
    };                                                                                  \


struct device *getIpcInstance(const struct ipc_device_wrapper *ipc_dev);

#define GET_IPC_INSTANCE(ipc_dev) \
    getIpcInstance(ipc_dev);

#endif /* IPC_SERVICE_DEFINES_H */
