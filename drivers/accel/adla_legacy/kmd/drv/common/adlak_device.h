/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_device.h
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

#ifndef __ADLAK_DEVICE_H__
#define __ADLAK_DEVICE_H__

/***************************** Include Files *********************************/
#include "adlak_common.h"
#include "adlak_hw.h"
#include "adlak_platform_device.h"
#include "adlak_queue.h"

#ifdef CONFIG_ADLAK_DEBUG_INNNER
#include "adlak_dbg.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
struct adlak_hardware_res {
	void *adlak_reg_va; /*mapped cpu virtual address for AHB register memory base address*/
	resource_size_t adlak_reg_pa; /*the AHB register physical base address*/
	u64 adlak_reg_size; /*the size of AHB register memory*/
	resource_size_t adlak_sram_pa; /*the AHB AXI SRAM physical base address*/
	u64 adlak_sram_size; /*the size of AXI SRAM memory*/
	s32 sram_wrap;

	resource_size_t
		adlak_resmem_pa; /*the reserved memory physical base address*/
	u64 adlak_resmem_size; /*the size of reserved memory*/

	struct io_region *preg;
	u32 irqline;
};

struct adlak_device_caps {
	void *data;
	size_t size;
};

struct adlak_proc_info {
	u32 irq_status;
};

struct adlak_parser_storage {
	u32 base_addr;
	u32 size;
	u32 rpt;
	u32 wpt;
	u32 ppt;
	u32 finish_id;
	u32 timestamp;
	u32 dbg_sw_id;
	u32 is_save_from_hw;
};

struct adlak_simple_bitmap {
	void *bitmap_pool;
	u32 size;
	u32 rpt;
};

struct adlak_circular_buffer {
	u32 *data;
	u32 size;
	u32 head;
	u32 tail;
};

struct adlak_cmq_buffer {
	u32 size;
	u32 cmq_wr_offset;
	u32 cmq_rd_offset;
	struct adlak_mem_handle *cmq_mm_info;
	struct adlak_parser_storage parser_storage_info;
};

struct adlak_device {
#ifndef CONFIG_ADLA_FREERTOS
	struct class *class;
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct miscdevice misc;
	dev_t devt;
	int major;
	struct clk *clk_axi;
	struct clk *clk_core;
	struct clk *clk;
	struct clk *clk_parent0;
	struct clk *clk_parent1;
	struct dentry *debugfs_parent;
#endif
	struct adlak_simple_bitmap net_id_bitmap;

	struct adlak_os_mutex_t *dev_mutex;
	struct adlak_os_spinlock_t *spinlock;
	struct adlak_hardware_res hw_res;
	struct adlak_device_caps dev_caps;
	struct adlak_workqueue queue;
	struct adlak_proc_info proc;
	struct list_head context_list;
	void *hw_info;
	u32 hw_timeout_ms; // unit is system tick
	int dependency_mode;
	int all_task_num;
	int all_task_num_max;
	bool is_clk_axi_enabled;
	bool is_clk_core_enabled;
	int clk_axi_freq_real;
	int clk_core_freq_real;
	int clk_axi_freq_set;
	int clk_core_freq_set;

	int is_suspend;
	int is_reset;
	/*Variables related to pm control*/
	int pm_suspend;
	struct adlak_os_sema_t *sem_pm_wakeup;

	struct adlak_cmq_buffer cmq_buffer_public;

	bool smmu_en;

	int dpm_en;
	int dpm_period_set;
	void *pdpm; // dynamic power management
	bool share_swap_en; /*Share swap memory between different models*/
	unsigned int share_buf_size;
	unsigned int iova_max_size_GB; // unit is Gbyte
	u32 save_time_en;
	u32 dev_hw_version; //Gops, base on
};

/**
 * @brief init adlak device
 *
 * @param padlak
 * @return int
 */
int adlak_device_init(struct adlak_device *padlak);
int adlak_device_deinit(struct adlak_device *padlak);

/**
 * @brief called in top-half IRQ handler
 *
 * @param padlak
 * @return int
 */
int adlak_irq_proc(struct adlak_device *const padlak);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_DEVICE_H__ end define*/
