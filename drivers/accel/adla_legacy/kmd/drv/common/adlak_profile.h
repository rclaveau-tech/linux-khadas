/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_profile.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/08/26	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_PROFILE_H__
#define __ADLAK_PROFILE_H__

/***************************** Include Files *********************************/

#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/
struct adlak_pm_cfg {
	s32 profile_en; // profilling enable
	u64 profile_iova;
	u32 profile_buf_size;
};

struct adlak_pm_state {
	int pm_wpt;
	int pm_rpt;
	struct adlak_os_ktime_t *start;
	struct adlak_os_ktime_t *finish;
};

struct adlak_profile {
	s32 profile_en; // profilling enable
	u32 time_elapsed_us;
};

/************************** Function Prototypes ******************************/

int adlak_profile_start(struct adlak_device *padlak,
			struct adlak_context *context,
			struct adlak_pm_cfg *pm_cfg,
			struct adlak_pm_state *pm_stat, s32 layer_start);

int adlak_profile_stop(struct adlak_device *padlak,
		       struct adlak_context *context,
		       struct adlak_pm_cfg *pm_cfg,
		       struct adlak_pm_state *pm_stat,
		       struct adlak_profile *profile_data, s32 layer_end);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_PROFILE_H__ end define*/
