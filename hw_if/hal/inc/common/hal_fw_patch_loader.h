/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @brief Header containing patch loader specific declarations for the
 * HAL Layer of the Wi-Fi driver.
 */

#ifndef __HAL_FW_PATCH_LOADER_H__
#define __HAL_FW_PATCH_LOADER_H__

#include "hal_structs_common.h"

enum nrf_wifi_fw_patch_type {
	NRF_WIFI_FW_PATCH_TYPE_PRI,
	NRF_WIFI_FW_PATCH_TYPE_SEC,
	NRF_WIFI_FW_PATCH_TYPE_MAX
};


/* Loads a firmware patch chunk into RPU memory. */
enum nrf_wifi_status hal_fw_patch_chunk_load(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						unsigned int dest_addr,
						const void *fw_chunk_data,
						unsigned int fw_chunk_size);
/*
 * Downloads a firmware patch into RPU memory.
 */
enum nrf_wifi_status nrf_wifi_hal_fw_patch_load(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						const void *fw_pri_patch_data,
						unsigned int fw_pri_patch_size,
						const void *fw_sec_patch_data,
						unsigned int fw_sec_patch_size);

enum nrf_wifi_status nrf_wifi_hal_fw_patch_boot(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						bool is_patch_present);

/**
 * @brief Read and verify firmware patches from RPU memory.
 *
 * This function reads patches from RPU memory and compares them with
 * expected patches.
 *
 * @param hal_dev_ctx Pointer to HAL device context.
 * @param rpu_proc RPU processor type.
 * @param exp_pri_patch_data Expected primary patch data.
 * @param exp_pri_patch_size Expected primary patch size.
 * @param exp_sec_patch_data Expected secondary patch data.
 * @param exp_sec_patch_size Expected secondary patch size.
 *
 * @return Status
 *         - Pass: NRF_WIFI_STATUS_SUCCESS
 *         - Error: NRF_WIFI_STATUS_FAIL
 */
enum nrf_wifi_status nrf_wifi_hal_fw_patch_verify(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						  enum RPU_PROC_TYPE rpu_proc,
						  const void *exp_pri_patch_data,
						  unsigned int exp_pri_patch_size,
						  const void *exp_sec_patch_data,
						  unsigned int exp_sec_patch_size);

/**
 * @brief Parse firmware and verify patches from RPU memory (HAL-only).
 *
 * This function parses the firmware binary to get expected patches using
 * HAL-only parsing (no FMAC dependency), then reads patches from RPU memory
 * and compares them.
 *
 * @param hal_dev_ctx Pointer to HAL device context.
 * @param rpu_proc RPU processor type.
 * @param fw_data Firmware binary data.
 * @param fw_size Firmware binary size.
 *
 * @return Status
 *         - Pass: NRF_WIFI_STATUS_SUCCESS
 *         - Error: NRF_WIFI_STATUS_FAIL
 */
enum nrf_wifi_status nrf_wifi_hal_fw_patch_verify_from_fw(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
							 enum RPU_PROC_TYPE rpu_proc,
							 const void *fw_data,
							 unsigned int fw_size);
#endif /* __HAL_FW_PATCH_LOADER_H__ */
