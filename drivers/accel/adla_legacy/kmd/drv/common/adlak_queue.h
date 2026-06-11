/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_queue.h
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

#ifndef __ADLAK_QUEUE_H__
#define __ADLAK_QUEUE_H__

/***************************** Include Files *********************************/
#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "../port/adlak_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct adlak_dev_inference {
	struct adlak_os_spinlock_t *spinlock;
	struct adlak_os_sema_t *sem_irq;
	struct adlak_os_timer_t *emu_timer;
	struct adlak_os_timer_t *dpm_timer;
	struct adlak_os_thread_t thrd_inference;
	unsigned int dpm_period_set;
	u32 wq_idle_cnt;
	u32 cnt_elapsed;
	u32 cnt_idle;
	u32 cnt_busy;
};

struct adlak_workqueue {
	struct adlak_device *padlak;
	struct adlak_os_mutex_t *wq_mutex;
	struct adlak_os_sema_t *wk_update;
	struct list_head pending_list; // invoke list
	struct list_head scheduled_list;
	struct list_head finished_list;

	int sched_num;
	int sched_num_max;
	int pending_num;
	int submit_num;
	int finished_num;
	void *ptask_sch_cur;
	void *cmq_buffer_pre; // the previous cmq buffer that submit

	struct adlak_dev_inference dev_inference;
};

/************************** Function Prototypes ******************************/

int adlak_queue_init(struct adlak_device *padlak);
int adlak_queue_deinit(struct adlak_device *padlak);
int adlak_queue_reset(struct adlak_device *padlak);

int adlak_debug_invoke_list_dump(struct adlak_device *padlak, u32 debug);

int adlak_dev_inference_init(struct adlak_device *padlak);
int adlak_dev_inference_deinit(struct adlak_device *padlak);
#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_QUEUE_H__ end define*/
