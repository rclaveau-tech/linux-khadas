/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_hw.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/05	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_HW_H__
#define __ADLAK_HW_H__

/***************************** Include Files *********************************/
#include "adlak_config.h"
#include "adlak_platform_config.h"
#include "adlak_reg.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/
#ifndef ADLAK_HW_DEBUG_EN
#define ADLAK_HW_DEBUG_EN (0)
#endif
#if (!(ADLAK_DEBUG))
#undef ADLAK_HW_DEBUG_EN
#define ADLAK_HW_DEBUG_EN (0)
#endif

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct adlak_hw_info {
	union {
		struct _revision {
			u8 minor;
			u8 major;
		} bitc;
		u32 all;
	} rev;
	struct _irq_cfg {
		u32 mask;
		u32 mask_err;
		u32 mask_normal;
	} irq_cfg;

	int reg_lst[REG_ADLAK_NUM_MAX];
	struct io_region *region;
};

struct adlak_irq_status {
	u32 irq_masked;
	u32 irq_raw;
	u32 time_stamp;
	bool timeout;
	u32 status_report;
};

struct adlak_hw_stat {
	u32 ps_finish_id;
	u32 ps_status;
	u32 ps_err_dat;
	u32 ps_idle_status;
	u32 ps_rbf_base;
	u32 ps_rbf_size;
	u32 ps_rbf_wpt;
	u32 ps_rbf_rpt;
	u32 ps_rbf_ppt;
	u32 smmu_err_dft_pa;
	u32 smmu_err_mdl_id;
	u32 smmu_err_iova;
	u32 ps_module_stat;
	u32 ps_dbg_id;
	u32 regs_stat[REG_ADLAK_NUM_MAX];
	struct adlak_irq_status irq_status;
	struct adlak_hw_info *hw_info;
};

/************************** Function Prototypes ******************************/

/**
 * struct io_region - a general struct describe IO region
 *
 * @phys: physical address base of an IO region
 * @kern: kernel virtual address base remapped from phys
 * @size: size of an IO region in byte
 */
struct io_region {
	u64 pa_kernel;
	u32 size;
	void *va_kernel;
};

#define ADLAK_BIT(data, n) (((data) >> (n)) & 0x1)

/**
 * @brief create ADLAK IO region using physical base address
 *
 * @param dev: device pointer
 * @param phys_base: base address
 * @param size: region size
 *
 * @return io_region pointer if successful; NULL if failed;
 */
struct io_region *adlak_create_ioregion(uintptr_t phys_base, u32 size);
/**
 * @brief destroy an ADLAK IO region
 *
 * @param region: region pointer created by adlak_create_ioregion
 */
void adlak_destroy_ioregion(struct io_region *region);

int      adlak_hw_init(void *data);
void     adlak_hw_deinit(void *data);
u32 adlak_get_hw_status(struct adlak_hw_stat *phw_stat);

struct adlak_irq_status *adlak_hal_get_irq_status(struct adlak_hw_stat *phw_stat);

int      adlak_save_cur_hw_info(void *data, void *phw_info_dest);
u32 adlak_hal_get_ps_rpt(void *data);

int adlak_hal_submit(void *data, u32 wpt);

void adlak_hal_smmu_cache_invalid(void *data, dma_addr_t iova);
int  adlak_hal_reset_and_start(void *data);
void adlak_hal_irq_clear(void *data, u32 clr_bits);
void adlak_status_dump(struct adlak_hw_stat *phw_stat);

int adlak_get_reg_name(int offset, char *buf, size_t buf_size);

void adlak_pm_reset(void *padlak);

void     adlak_pm_config(void *padlak, u32 addr, u32 buf_size, u32 wpt);
u32 adlak_pm_get_stat(void *padlak);
int      adlak_pm_fush_until_empty(void *padlak);
void     adlak_pm_enable(void *padlak, u32 en);
void     adlak_hal_irq_enable(void *padlak, u32 en);

void adlak_hw_dev_resume(void *data);
void adlak_hw_dev_suspend(void *data);

int adlak_check_dev_is_idle(void *data);

int adlak_hal_parser_resume(void *data, void *parser_storage);

int adlak_hal_save_parser_info(void *data, void *parser_storage);

int adlak_hal_check_preempt_is_done(void *data);

void     adlak_hal_set_preempt(void *data);
u32 adlak_hal_get_reg(void *data, u32 offset);
int      adlak_hal_set_mmu(void *data, bool enable, u64 smmu_entry);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_HW_H__ end define*/
