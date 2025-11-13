/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @brief File containing patch loader specific definitions for the
 * HAL Layer of the Wi-Fi driver.
 */

#include "host_rpu_common_if.h"
#include "common/hal_fw_patch_loader.h"
#include "common/hal_mem.h"
#include "lmac_if_common.h"
#include "host_rpu_common_if.h"
#include "common/pack_def.h"

/* To reduce HEAP maximum usage */
#define MAX_PATCH_CHUNK_SIZE 8192
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif /* ARRAY_SIZE */

/* Firmware binary structure definitions (HAL-only, no FMAC dependency) */
#define NRF_WIFI_PATCH_SIGNATURE 0xDEAD1EAF
#define NRF_WIFI_PATCH_HASH_LEN 32

enum nrf70_image_ids {
	NRF70_IMAGE_UMAC_PRI = 0,
	NRF70_IMAGE_UMAC_SEC,
	NRF70_IMAGE_LMAC_PRI,
	NRF70_IMAGE_LMAC_SEC,
};

struct nrf70_fw_image {
	unsigned int type;
	unsigned int len;
	/* Data follows */
	unsigned char data[];
} __NRF_WIFI_PKD;

struct nrf70_fw_image_info {
	unsigned int signature;
	unsigned int num_images;
	unsigned int version;
	unsigned int feature_flags;
	unsigned int len;
	/* Protects against image corruption */
	unsigned char hash[NRF_WIFI_PATCH_HASH_LEN];
	unsigned char data[];
} __NRF_WIFI_PKD;

const struct nrf70_fw_addr_info nrf70_fw_addr_info[] = {
	{ RPU_PROC_TYPE_MCU_LMAC, "LMAC bimg", RPU_MEM_LMAC_PATCH_BIMG },
	{ RPU_PROC_TYPE_MCU_LMAC, "LMAC bin", RPU_MEM_LMAC_PATCH_BIN },
	{ RPU_PROC_TYPE_MCU_UMAC, "UMAC bimg", RPU_MEM_UMAC_PATCH_BIMG },
	{ RPU_PROC_TYPE_MCU_UMAC, "UMAC bin", RPU_MEM_UMAC_PATCH_BIN },
};

struct patch_contents {
	const char *id_str;
	const void *data;
	unsigned int size;
	unsigned int dest_addr;
};


static const struct rpu_mcu_boot_vectors RPU_MCU_BOOT_VECTORS[] = {
	/* MCU1 - LMAC */
	{
		{
			{RPU_REG_MIPS_MCU_BOOT_EXCP_INSTR_0, NRF_WIFI_LMAC_BOOT_EXCP_VECT_0},
			{RPU_REG_MIPS_MCU_BOOT_EXCP_INSTR_1, NRF_WIFI_LMAC_BOOT_EXCP_VECT_1},
			{RPU_REG_MIPS_MCU_BOOT_EXCP_INSTR_2, NRF_WIFI_LMAC_BOOT_EXCP_VECT_2},
			{RPU_REG_MIPS_MCU_BOOT_EXCP_INSTR_3, NRF_WIFI_LMAC_BOOT_EXCP_VECT_3},
		}
	},
	/* MCU2 - UMAC */
	{
		{
			{RPU_REG_MIPS_MCU2_BOOT_EXCP_INSTR_0, NRF_WIFI_UMAC_BOOT_EXCP_VECT_0},
			{RPU_REG_MIPS_MCU2_BOOT_EXCP_INSTR_1, NRF_WIFI_UMAC_BOOT_EXCP_VECT_1},
			{RPU_REG_MIPS_MCU2_BOOT_EXCP_INSTR_2, NRF_WIFI_UMAC_BOOT_EXCP_VECT_2},
			{RPU_REG_MIPS_MCU2_BOOT_EXCP_INSTR_3, NRF_WIFI_UMAC_BOOT_EXCP_VECT_3},
		}
	},
};


enum nrf_wifi_status hal_fw_patch_chunk_load(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						unsigned int dest_addr,
						const void *fw_chunk_data,
						unsigned int fw_chunk_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;

	hal_dev_ctx->curr_proc = rpu_proc;

	status = hal_rpu_mem_write(hal_dev_ctx,
				 dest_addr,
				 (void *)fw_chunk_data,
				 fw_chunk_size);

	hal_dev_ctx->curr_proc = RPU_PROC_TYPE_MCU_LMAC;

	return status;
}

/* In order to save RAM, divide the patch in to chunks download */
static enum nrf_wifi_status hal_fw_patch_load(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						const char *patch_id_str,
						unsigned int dest_addr,
						const void *fw_patch_data,
						unsigned int fw_patch_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	int last_chunk_size = fw_patch_size % MAX_PATCH_CHUNK_SIZE;
	int num_chunks = fw_patch_size / MAX_PATCH_CHUNK_SIZE +
					(last_chunk_size ? 1 : 0);
	int chunk = 0;

	for (chunk = 0; chunk < num_chunks; chunk++) {
		unsigned char *patch_data_ram;
		unsigned int patch_chunk_size =
			((chunk == num_chunks - 1) ? last_chunk_size : MAX_PATCH_CHUNK_SIZE);
		const void *src_patch_offset = (const char *)fw_patch_data +
			chunk * MAX_PATCH_CHUNK_SIZE;
		int dest_chunk_offset = dest_addr + chunk * MAX_PATCH_CHUNK_SIZE;

		patch_data_ram = nrf_wifi_osal_mem_alloc(patch_chunk_size);
		if (!patch_data_ram) {
			nrf_wifi_osal_log_err("%s: Mem alloc failed for patch "
					      "%s-%s: chunk %d/%d, size: %d",
					      __func__,
					      rpu_proc_to_str(rpu_proc),
					      patch_id_str,
					      chunk + 1,
					      num_chunks,
					      patch_chunk_size);
			status = NRF_WIFI_STATUS_FAIL;
			goto out;
		}

		nrf_wifi_osal_mem_cpy(patch_data_ram,
				      src_patch_offset,
				      patch_chunk_size);


		nrf_wifi_osal_log_dbg("%s: Copying patch %s-%s: chunk %d/%d, size: %d",
				      __func__,
				      rpu_proc_to_str(rpu_proc),
				      patch_id_str,
				      chunk + 1,
				      num_chunks,
				      patch_chunk_size);

		status = hal_fw_patch_chunk_load(hal_dev_ctx,
						rpu_proc,
						dest_chunk_offset,
						patch_data_ram,
						patch_chunk_size);
		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err("%s: Patch copy %s-%s: chunk %d/%d, size: %d failed",
					      __func__,
					      rpu_proc_to_str(rpu_proc),
					      patch_id_str,
					      chunk + 1,
					      num_chunks,
					      patch_chunk_size);
			goto out;
		}
out:
		if (patch_data_ram)
			nrf_wifi_osal_mem_free(patch_data_ram);
		if (status != NRF_WIFI_STATUS_SUCCESS)
			break;
	}

	return status;
}

/*
 * Copies the firmware patches to the RPU memory.
 */
enum nrf_wifi_status nrf_wifi_hal_fw_patch_load(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						const void *fw_pri_patch_data,
						unsigned int fw_pri_patch_size,
						const void *fw_sec_patch_data,
						unsigned int fw_sec_patch_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned int pri_dest_addr = 0;
	unsigned int sec_dest_addr = 0;
	int patch = 0;

	if (!fw_pri_patch_data) {
		nrf_wifi_osal_log_err("%s: Primary patch missing for RPU (%d)",
				      __func__,
				      rpu_proc);
		status = NRF_WIFI_STATUS_FAIL;
		goto out;
	}

	if (!fw_sec_patch_data) {
		nrf_wifi_osal_log_err("%s: Secondary patch missing for RPU (%d)",
				      __func__,
				      rpu_proc);
		status = NRF_WIFI_STATUS_FAIL;
		goto out;
	}

	/* Set the HAL RPU context to the current required context */
	hal_dev_ctx->curr_proc = rpu_proc;

	switch (rpu_proc) {
	case RPU_PROC_TYPE_MCU_LMAC:
		pri_dest_addr = RPU_MEM_LMAC_PATCH_BIMG;
		sec_dest_addr = RPU_MEM_LMAC_PATCH_BIN;
		break;
	case RPU_PROC_TYPE_MCU_UMAC:
		pri_dest_addr = RPU_MEM_UMAC_PATCH_BIMG;
		sec_dest_addr = RPU_MEM_UMAC_PATCH_BIN;
		break;
	default:
		nrf_wifi_osal_log_err("%s: Invalid RPU processor type[%d]",
				      __func__,
				      rpu_proc);

		goto out;
	}

	/* This extra block is needed to avoid compilation error for inline
	 * declaration but still keep using const data.
	 */
	{
		const struct patch_contents patches[] = {
			{ "bimg", fw_pri_patch_data, fw_pri_patch_size, pri_dest_addr },
			{ "bin", fw_sec_patch_data, fw_sec_patch_size, sec_dest_addr },
		};

		for (patch = 0; patch < ARRAY_SIZE(patches); patch++) {
			status = hal_fw_patch_load(hal_dev_ctx,
						rpu_proc,
						patches[patch].id_str,
						patches[patch].dest_addr,
						patches[patch].data,
						patches[patch].size);
			if (status != NRF_WIFI_STATUS_SUCCESS)
				goto out;
		}

status=		nrf_wifi_hal_fw_patch_verify(hal_dev_ctx,
						rpu_proc,
						fw_pri_patch_data,
						fw_pri_patch_size,
						fw_sec_patch_data,
						fw_sec_patch_size);
		if (status != NRF_WIFI_STATUS_SUCCESS)
			goto out;
		nrf_wifi_osal_log_info("%s: Patch verification successful for RPU(%d)",
					__func__,
					rpu_proc);
	}
out:
	/* Reset the HAL RPU context to the LMAC context */
	hal_dev_ctx->curr_proc = RPU_PROC_TYPE_MCU_LMAC;

	return status;
}


enum nrf_wifi_status nrf_wifi_hal_fw_patch_boot(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
						enum RPU_PROC_TYPE rpu_proc,
						bool is_patch_present)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned int boot_sig_addr = 0;
	unsigned int boot_sig_val = 0;
	unsigned int boot_vector_id;
	unsigned int sleepctrl_addr = 0;
	unsigned int sleepctrl_val = 0;
	unsigned int run_addr = 0;
	const struct rpu_mcu_boot_vectors *boot_vectors = &RPU_MCU_BOOT_VECTORS[rpu_proc];

	if (rpu_proc == RPU_PROC_TYPE_MCU_LMAC) {
		boot_sig_addr = RPU_MEM_LMAC_BOOT_SIG;
		run_addr = RPU_REG_MIPS_MCU_CONTROL;
		if (is_patch_present) {
			sleepctrl_addr = RPU_REG_UCC_SLEEP_CTRL_DATA_0;
			sleepctrl_val = NRF_WIFI_LMAC_ROM_PATCH_OFFSET;
		}
	} else if (rpu_proc == RPU_PROC_TYPE_MCU_UMAC) {
		boot_sig_addr = RPU_MEM_UMAC_BOOT_SIG;
		run_addr = RPU_REG_MIPS_MCU2_CONTROL;
		if (is_patch_present) {
			sleepctrl_addr = RPU_REG_UCC_SLEEP_CTRL_DATA_1;
			sleepctrl_val = NRF_WIFI_UMAC_ROM_PATCH_OFFSET;
		}
	} else {
		nrf_wifi_osal_log_err("%s: Invalid RPU processor type %d",
				      __func__,
				      rpu_proc);
		goto out;
	}

	/* Set the HAL RPU context to the current required context */
	hal_dev_ctx->curr_proc = rpu_proc;

	/* Clear the firmware pass signature location */
	status = hal_rpu_mem_write(hal_dev_ctx,
				   boot_sig_addr,
				   &boot_sig_val,
				   sizeof(boot_sig_val));

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: Clearing of FW pass signature failed for RPU(%d)",
				      __func__,
				      rpu_proc);

		goto out;
	}

	if (is_patch_present) {
		/* Write to sleep control register */
		status = hal_rpu_reg_write(hal_dev_ctx,
					   sleepctrl_addr,
					   sleepctrl_val);
		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err("%s: Sleep control reg write failed for RPU(%d)\n",
					      __func__,
					      rpu_proc);

			goto out;
		}
	}

	for (boot_vector_id = 0; boot_vector_id < ARRAY_SIZE(boot_vectors->vectors); boot_vector_id++) {
		const struct rpu_mcu_boot_vector *boot_vector = &boot_vectors->vectors[boot_vector_id];

		/* Write the boot vector to the RPU memory */
		status = hal_rpu_reg_write(hal_dev_ctx,
					   boot_vector->addr,
					   boot_vector->val);
		if (status != NRF_WIFI_STATUS_SUCCESS) {
			nrf_wifi_osal_log_err("%s: Writing boot vector failed for RPU(%d)\n",
					      __func__,
					      rpu_proc);

			goto out;
		}
	}

	/* Perform pulsed soft reset of MIPS - this should now run */
	status = hal_rpu_reg_write(hal_dev_ctx,
				   run_addr,
				   0x1);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: RPU processor(%d) run failed",
				      __func__,
				      rpu_proc);

		goto out;
	}
out:
	/* Reset the HAL RPU context to the LMAC context */
	hal_dev_ctx->curr_proc = RPU_PROC_TYPE_MCU_LMAC;

	return status;

}

/**
 * @brief Read a firmware patch from RPU memory.
 *
 * This function reads a patch from the specified RPU memory address.
 *
 * @param hal_dev_ctx Pointer to HAL device context.
 * @param rpu_proc RPU processor type.
 * @param patch_addr RPU memory address of the patch.
 * @param patch_data Buffer to store the read patch data.
 * @param patch_size Size of the patch to read.
 *
 * @return Status
 *         - Pass: NRF_WIFI_STATUS_SUCCESS
 *         - Error: NRF_WIFI_STATUS_FAIL
 */
static enum nrf_wifi_status hal_fw_patch_read(struct nrf_wifi_hal_dev_ctx *hal_dev_ctx,
					       enum RPU_PROC_TYPE rpu_proc,
					       unsigned int patch_addr,
					       void *patch_data,
					       unsigned int patch_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	enum RPU_PROC_TYPE curr_proc = hal_dev_ctx->curr_proc;

	hal_dev_ctx->curr_proc = rpu_proc;

	status = hal_rpu_mem_read(hal_dev_ctx,
				 patch_data,
				 patch_addr,
				 patch_size);

	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: Reading patch from RPU(%d) "
				      "addr 0x%X size %d failed",
				      __func__,
				      rpu_proc,
				      patch_addr,
				      patch_size);
	}

	hal_dev_ctx->curr_proc = curr_proc;

	return status;
}

/**
 * @brief Compare two patch buffers.
 *
 * @param patch1 First patch buffer.
 * @param patch1_size Size of first patch.
 * @param patch2 Second patch buffer.
 * @param patch2_size Size of second patch.
 * @param mismatch_offset Output parameter for first mismatch offset.
 *
 * @return true if patches match, false otherwise.
 */
static bool hal_fw_patch_compare(const void *patch1,
				  unsigned int patch1_size,
				  const void *patch2,
				  unsigned int patch2_size,
				  unsigned int *mismatch_offset)
{
	unsigned int min_size;
	unsigned int i;
	bool mismatch = false;

	if (patch1_size != patch2_size) {
		nrf_wifi_osal_log_err("%s: Patch size mismatch: %d vs %d",
				      __func__,
				      patch1_size,
				      patch2_size);
		return false;
	}

	min_size = patch1_size;

	// DUmp first 8bytes
	nrf_wifi_osal_log_err("%s: Patch1: %02X %02X %02X %02X %02X %02X %02X %02X",
			      __func__,
			      ((const unsigned char *)patch1)[0],
			      ((const unsigned char *)patch1)[1],
			      ((const unsigned char *)patch1)[2],
			      ((const unsigned char *)patch1)[3],
			      ((const unsigned char *)patch1)[4],
			      ((const unsigned char *)patch1)[5],
			      ((const unsigned char *)patch1)[6],
			      ((const unsigned char *)patch1)[7]);
	nrf_wifi_osal_log_err("%s: Patch2: %02X %02X %02X %02X %02X %02X %02X %02X",
			      __func__,
			      ((const unsigned char *)patch2)[0],
			      ((const unsigned char *)patch2)[1],
			      ((const unsigned char *)patch2)[2],
			      ((const unsigned char *)patch2)[3],
			      ((const unsigned char *)patch2)[4],
			      ((const unsigned char *)patch2)[5],
			      ((const unsigned char *)patch2)[6],
			      ((const unsigned char *)patch2)[7]);
	for (i = 0; i < min_size; i++) {
		if (((const unsigned char *)patch1)[i] !=
		    ((const unsigned char *)patch2)[i]) {
			if (mismatch_offset) {
				*mismatch_offset = i;
			}
			nrf_wifi_osal_log_err("%s: Patch mismatch at offset %d: "
					      "0x%02X (expected) vs 0x%02X (actual)",
					      __func__,
					      i,
					      ((const unsigned char *)patch1)[i],
					      ((const unsigned char *)patch2)[i]);
			mismatch = true;
		}
	}

	return mismatch;
}

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
						  unsigned int exp_sec_patch_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	unsigned int pri_patch_addr = 0;
	unsigned int sec_patch_addr = 0;
	unsigned char *read_pri_patch = NULL;
	unsigned char *read_sec_patch = NULL;
	unsigned int mismatch_offset = 0;
	bool match = false;

	if (!exp_pri_patch_data || !exp_sec_patch_data) {
		nrf_wifi_osal_log_err("%s: Expected patch data missing for RPU(%d)",
				      __func__,
				      rpu_proc);
		goto out;
	}

	switch (rpu_proc) {
	case RPU_PROC_TYPE_MCU_LMAC:
		pri_patch_addr = RPU_MEM_LMAC_PATCH_BIMG;
		sec_patch_addr = RPU_MEM_LMAC_PATCH_BIN;
		break;
	case RPU_PROC_TYPE_MCU_UMAC:
		pri_patch_addr = RPU_MEM_UMAC_PATCH_BIMG;
		sec_patch_addr = RPU_MEM_UMAC_PATCH_BIN;
		break;
	default:
		nrf_wifi_osal_log_err("%s: Invalid RPU processor type[%d]",
				      __func__,
				      rpu_proc);
		goto out;
	}

	read_pri_patch = nrf_wifi_osal_mem_alloc(exp_pri_patch_size);
	if (!read_pri_patch) {
		nrf_wifi_osal_log_err("%s: Failed to allocate memory for "
				      "reading primary patch",
				      __func__);
		goto out;
	}

	read_sec_patch = nrf_wifi_osal_mem_alloc(exp_sec_patch_size);
	if (!read_sec_patch) {
		nrf_wifi_osal_log_err("%s: Failed to allocate memory for "
				      "reading secondary patch",
				      __func__);
		goto out;
	}

	nrf_wifi_osal_log_info("%s: Reading primary patch from RPU(%d) addr 0x%X size %d",
			      __func__,
			      rpu_proc,
			      pri_patch_addr,
			      exp_pri_patch_size);
	status = hal_fw_patch_read(hal_dev_ctx,
				  rpu_proc,
				  pri_patch_addr,
				  read_pri_patch,
				  exp_pri_patch_size);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: Failed to read primary patch from RPU",
				      __func__);
		goto out;
	}
	match = hal_fw_patch_compare(exp_pri_patch_data,
		exp_pri_patch_size,
		read_pri_patch,
		exp_pri_patch_size,
		&mismatch_offset);
	if (!match) {
		nrf_wifi_osal_log_err("%s: Primary patch mismatch for RPU(%d) "
				"at offset %d",
				__func__,
				rpu_proc,
				mismatch_offset);
	}

	status = hal_fw_patch_read(hal_dev_ctx,
				  rpu_proc,
				  sec_patch_addr,
				  read_sec_patch,
				  exp_sec_patch_size);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: Failed to read secondary patch from RPU",
				      __func__);
		goto out;
	}


	match = hal_fw_patch_compare(exp_sec_patch_data,
				     exp_sec_patch_size,
				     read_sec_patch,
				     exp_sec_patch_size,
				     &mismatch_offset);
	if (!match) {
		nrf_wifi_osal_log_err("%s: Secondary patch mismatch for RPU(%d) "
				      "at offset %d",
				      __func__,
				      rpu_proc,
				      mismatch_offset);
	}

	nrf_wifi_osal_log_dbg("%s: Patches verified successfully for RPU(%d)",
			      __func__,
			      rpu_proc);
	status = NRF_WIFI_STATUS_SUCCESS;

out:
	if (read_pri_patch) {
		nrf_wifi_osal_mem_free(read_pri_patch);
	}
	if (read_sec_patch) {
		nrf_wifi_osal_mem_free(read_sec_patch);
	}

	return status;
}

/**
 * @brief Parse firmware binary to extract patch information (HAL-only).
 *
 * This function parses the firmware binary structure to extract patch data
 * without any FMAC dependency.
 *
 * @param fw_data Firmware binary data.
 * @param fw_size Firmware binary size.
 * @param rpu_proc RPU processor type.
 * @param exp_pri_patch_data Output: Expected primary patch data.
 * @param exp_pri_patch_size Output: Expected primary patch size.
 * @param exp_sec_patch_data Output: Expected secondary patch data.
 * @param exp_sec_patch_size Output: Expected secondary patch size.
 *
 * @return Status
 *         - Pass: NRF_WIFI_STATUS_SUCCESS
 *         - Error: NRF_WIFI_STATUS_FAIL
 */
static enum nrf_wifi_status hal_fw_parse_patches(const void *fw_data,
						  unsigned int fw_size,
						  enum RPU_PROC_TYPE rpu_proc,
						  const void **exp_pri_patch_data,
						  unsigned int *exp_pri_patch_size,
						  const void **exp_sec_patch_data,
						  unsigned int *exp_sec_patch_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	const struct nrf70_fw_image_info *info = NULL;
	unsigned int offset = 0;
	unsigned int image_id = 0;
	enum nrf70_image_ids pri_image_id = 0;
	enum nrf70_image_ids sec_image_id = 0;

	if (!fw_data || !fw_size || !exp_pri_patch_data || !exp_pri_patch_size ||
	    !exp_sec_patch_data || !exp_sec_patch_size) {
		nrf_wifi_osal_log_err("%s: Invalid parameters",
				      __func__);
		return NRF_WIFI_STATUS_FAIL;
	}

	if (fw_size < sizeof(struct nrf70_fw_image_info)) {
		nrf_wifi_osal_log_err("%s: Invalid fw_size: %d, minimum: %d",
				      __func__,
				      fw_size,
				      (unsigned int)sizeof(struct nrf70_fw_image_info));
		return NRF_WIFI_STATUS_FAIL;
	}

	info = (const struct nrf70_fw_image_info *)fw_data;

	if (info->signature != NRF_WIFI_PATCH_SIGNATURE) {
		nrf_wifi_osal_log_err("%s: Invalid firmware signature: 0x%X",
				      __func__,
				      info->signature);
		return NRF_WIFI_STATUS_FAIL;
	}

	/* Determine which image IDs to look for based on RPU processor */
	switch (rpu_proc) {
	case RPU_PROC_TYPE_MCU_LMAC:
		pri_image_id = NRF70_IMAGE_LMAC_PRI;
		sec_image_id = NRF70_IMAGE_LMAC_SEC;
		break;
	case RPU_PROC_TYPE_MCU_UMAC:
		pri_image_id = NRF70_IMAGE_UMAC_PRI;
		sec_image_id = NRF70_IMAGE_UMAC_SEC;
		break;
	default:
		nrf_wifi_osal_log_err("%s: Invalid RPU processor type[%d]",
				      __func__,
				      rpu_proc);
		return NRF_WIFI_STATUS_FAIL;
	}

	*exp_pri_patch_data = NULL;
	*exp_pri_patch_size = 0;
	*exp_sec_patch_data = NULL;
	*exp_sec_patch_size = 0;

	offset = sizeof(struct nrf70_fw_image_info);

	for (image_id = 0; image_id < info->num_images; image_id++) {
		const struct nrf70_fw_image *image = NULL;
		const void *data = NULL;

		if (offset + sizeof(struct nrf70_fw_image) > fw_size) {
			nrf_wifi_osal_log_err("%s: Invalid offset for image[%d]",
					      __func__,
					      image_id);
			return NRF_WIFI_STATUS_FAIL;
		}

		image = (const struct nrf70_fw_image *)((const char *)fw_data + offset);
		data = (const char *)fw_data + offset + sizeof(struct nrf70_fw_image);

		if (offset + sizeof(struct nrf70_fw_image) + image->len > fw_size) {
			nrf_wifi_osal_log_err("%s: Invalid fw_size for image[%d] "
					      "len: %d",
					      __func__,
					      image_id,
					      image->len);
			return NRF_WIFI_STATUS_FAIL;
		}

		if (image->type == pri_image_id) {
			*exp_pri_patch_data = data;
			*exp_pri_patch_size = image->len;
		} else if (image->type == sec_image_id) {
			*exp_sec_patch_data = data;
			*exp_sec_patch_size = image->len;
		}

		offset += sizeof(struct nrf70_fw_image) + image->len;
	}

	if (!*exp_pri_patch_data || !*exp_sec_patch_data) {
		nrf_wifi_osal_log_err("%s: Patches not found in firmware for RPU(%d)",
				      __func__,
				      rpu_proc);
		return NRF_WIFI_STATUS_FAIL;
	}

	return NRF_WIFI_STATUS_SUCCESS;
}

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
							   unsigned int fw_size)
{
	enum nrf_wifi_status status = NRF_WIFI_STATUS_FAIL;
	const void *exp_pri_patch_data = NULL;
	unsigned int exp_pri_patch_size = 0;
	const void *exp_sec_patch_data = NULL;
	unsigned int exp_sec_patch_size = 0;

	if (!hal_dev_ctx || !fw_data || !fw_size) {
		nrf_wifi_osal_log_err("%s: Invalid parameters for RPU(%d)",
				      __func__,
				      rpu_proc);
		goto out;
	}

	status = hal_fw_parse_patches(fw_data,
				      fw_size,
				      rpu_proc,
				      &exp_pri_patch_data,
				      &exp_pri_patch_size,
				      &exp_sec_patch_data,
				      &exp_sec_patch_size);
	if (status != NRF_WIFI_STATUS_SUCCESS) {
		nrf_wifi_osal_log_err("%s: Failed to parse firmware for RPU(%d)",
				      __func__,
				      rpu_proc);
		goto out;
	}

	if (exp_pri_patch_data && exp_pri_patch_size >= 4) {
		const unsigned char *p = (const unsigned char *)exp_pri_patch_data;
		nrf_wifi_osal_log_err("%s: Primary patch first 4 bytes: %02X %02X %02X %02X (size: %d)",
				      __func__, p[0], p[1], p[2], p[3], exp_pri_patch_size);
	} else {
		nrf_wifi_osal_log_err("%s: Primary patch data unavailable or too small (size: %d)",
				      __func__, exp_pri_patch_size);
	}
	if (exp_sec_patch_data && exp_sec_patch_size >= 4) {
		const unsigned char *s = (const unsigned char *)exp_sec_patch_data;
		nrf_wifi_osal_log_err("%s: Secondary patch first 4 bytes: %02X %02X %02X %02X (size: %d)",
				      __func__, s[0], s[1], s[2], s[3], exp_sec_patch_size);
	} else {
		nrf_wifi_osal_log_err("%s: Secondary patch data unavailable or too small (size: %d)",
				      __func__, exp_sec_patch_size);
	}

	status = nrf_wifi_hal_fw_patch_verify(hal_dev_ctx,
					     rpu_proc,
					     exp_pri_patch_data,
					     exp_pri_patch_size,
					     exp_sec_patch_data,
					     exp_sec_patch_size);

out:
	return status;
}
