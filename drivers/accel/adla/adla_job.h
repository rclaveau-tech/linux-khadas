// SPDX-License-Identifier: GPL-2.0-only

#ifndef __ADLA_JOB_H__
#define __ADLA_JOB_H__

#include <drm/drm.h>
#include <drm/gpu_scheduler.h>

/***************************** RING BUFFER **********************************/

/* Parser commands */
#define ADLA_CMD_NOP			0x70000000
#define ADLA_CMD_RESET_ID		0x71000000
#define ADLA_CMD_SET_FENCE		0x72000000
#define ADLA_CMD_SET_TIME_STAMP		0x73000000
#define ADLA_CMD_SET_DEPENDENCY		0x74000000
#define ADLA_CMD_EXECUTE		0x75000000
#define ADLA_CMD_CONFIG			0x76000000
#define ADLA_CMD_CONFIG_WITH_ADDRESS	0x77000000
#define ADLA_CMD_SET_SW_ID		0x7E000000
#define ADLA_CMD_STOP			0x7F000000

#define ADLA_CMD_ALIGN			16 /* bytes */

/* FENCE modules */
#define ADLA_FENCE_PWX			BIT(20)
#define ADLA_FENCE_PWE			BIT(21)
#define ADLA_FENCE_RS			BIT(22)

/* TIME_STAMP */
#define ADLA_TIME_STAMP_IRQ		BIT(0)

/* DEPENDENCY */
#define ADLA_DEP_PWX_VALID		BIT(20)
#define ADLA_DEP_PWE_VALID		BIT(21)
#define ADLA_DEP_RS_VALID		BIT(22)
#define ADLA_DEP_PWX_ID_MASK		0x0000000f
#define ADLA_DEP_PWX_ID_SHIFT		0
#define ADLA_DEP_PWE_ID_MASK		0x000000f0
#define ADLA_DEP_PWE_ID_SHIFT		4
#define ADLA_DEP_RS_ID_MASK		0x00000f00
#define ADLA_DEP_RS_ID_SHIFT		8

/* EXECUTE output modules */
#define ADLA_EXECUTE_PWX		BIT(20)
#define ADLA_EXECUTE_PWE		BIT(21)
#define ADLA_EXECUTE_RS			BIT(22)

/* CONFIG active modules */
#define ADLA_CONFIG_PWX			BIT(0)
#define ADLA_CONFIG_PWE			BIT(1)
#define ADLA_CONFIG_PX			BIT(2)
#define ADLA_CONFIG_DMDW		BIT(3)
#define ADLA_CONFIG_DMDF		BIT(4)
#define ADLA_CONFIG_DW			BIT(5)
#define ADLA_CONFIG_PE			BIT(6)
#define ADLA_CONFIG_DMCW		BIT(7)
#define ADLA_CONFIG_DMCF		BIT(8)
#define ADLA_CONFIG_MC			BIT(9)
#define ADLA_CONFIG_RS			BIT(10)
#define ADLA_CONFIG_PX_LUT		BIT(20)
#define ADLA_CONFIG_PE_LUT		BIT(21)
#define ADLA_CONFIG_RS_CAT		BIT(22)

/* SW_ID */
#define ADLA_SW_ID_MASK			0x00ffffff

struct adla_fixup {
	u32 blob_offset;
	u32 pad;
	u64 iova;
};

struct adla_job {
	struct drm_sched_job base;

	struct adla_device *adla;
	struct dma_fence *hw_fence;
	struct dma_fence *done_fence;

	u32 id;

	/* modules */
	u32 output_modules;
	u32 active_modules;
	u32 fence_modules;
	u32 dep;

	u32 *config;
	u32 config_size;

	struct adla_fixup *fixups;
	u32 fixup_count;
};

int adla_ioctl_submit(struct drm_device *dev, void *data, struct drm_file *filp);
int adla_ioctl_wait(struct drm_device *dev, void *data, struct drm_file *filp);
int adla_job_init(struct device *dev, struct adla_device *adla);
void adla_job_exit(void);

#endif //__ADLA_JOB_H__
