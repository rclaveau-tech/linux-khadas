// SPDX-License-Identifier: GPL-2.0-only

#ifndef __ADLA_DRV_H__
#define __ADLA_DRV_H__

#include <drm/drm_device.h>
#include <drm/gpu_scheduler.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

/* REV - 0x00 */
#define REG_ADLA_REVISION		0x00000000
#define ADLA_REVISION_MINOR__MASK	0x000000FF
#define ADLA_REVISION_MINOR__SHIFT	0
#define ADLA_REVISION_MAJOR__MASK	0x0000FF00
#define ADLA_REVISION_MAJOR__SHIFT	8

static inline uint32_t ADLA_REVISION_MINOR(uint32_t val)
{
	return ((val) << ADLA_REVISION_MINOR__SHIFT) & ADLA_REVISION_MINOR__MASK;
}
static inline uint32_t ADLA_REVISION_MAJOR(uint32_t val)
{
	return ((val) << ADLA_REVISION_MAJOR__SHIFT) & ADLA_REVISION_MAJOR__MASK;
}

/* APB_TIMEOUT - 0x04 */
#define REG_ADLA_PS_APB_TIMEOUT		0x00000004

/* IRQ - 0x10..0x1C */
#define REG_ADLA_IRQ_MASKED		0x00000010
#define REG_ADLA_IRQ_MASK		0x00000014
#define REG_ADLA_IRQ_RAW		0x00000018
#define ADLA_IRQ_PARSER_STOP_CMD	BIT(0)
#define ADLA_IRQ_PARSER_STOP_ERR	BIT(1)
#define ADLA_IRQ_PARSER_STOP_PMT	BIT(2)
#define ADLA_IRQ_PEND_TIMEOUT		BIT(3)
#define ADLA_IRQ_LAYER_END		BIT(4)
#define ADLA_IRQ_TIME_STAMP		BIT(5)
#define ADLA_IRQ_APB_WAIT_TIMEOUT	BIT(6)
#define ADLA_IRQ_PM_DRAM_OVF		BIT(7)
#define ADLA_IRQ_PM_FIFO_OVF		BIT(8)
#define ADLA_IRQ_PM_ARBITER_OVF		BIT(9)
#define ADLA_IRQ_INVALID_IOVA		BIT(10)

/* STS_REPORT - 0x1C */
#define REG_ADLA_STS_REPORT		0x0000001C
#define ADLA_STS_HANG_DW_SRAMF		BIT(0)
#define ADLA_STS_HANG_DW_SRAMW		BIT(1)
#define ADLA_STS_HANG_PE_SRAMA		BIT(2)
#define ADLA_STS_HANG_PE_SRAMM		BIT(3)
#define ADLA_STS_HANG_PX_SRAMA		BIT(4)
#define ADLA_STS_HANG_PX_SRAMM		BIT(5)
/* bit 6 reserved */
#define ADLA_STS_HANG_VLC_DECODER	BIT(7)
/* bits [15:8] = vlc_decoder_rpid */
#define ADLA_STS_HANG_PS_DEP		BIT(16)
#define ADLA_STS_HANG_MC_DEP		BIT(17)
#define ADLA_STS_HANG_DW_F_DEP		BIT(18)
#define ADLA_STS_HANG_DW_W_DEP		BIT(19)
#define ADLA_STS_HANG_RS_DEP		BIT(20)

#define ADLA_STS_HANG_MASK (ADLA_STS_HANG_DW_SRAMF  | \
			    ADLA_STS_HANG_DW_SRAMW  | \
			    ADLA_STS_HANG_PE_SRAMA  | \
			    ADLA_STS_HANG_PE_SRAMM  | \
			    ADLA_STS_HANG_PX_SRAMA  | \
			    ADLA_STS_HANG_PX_SRAMM  | \
			    ADLA_STS_HANG_VLC_DECODER | \
			    ADLA_STS_HANG_PS_DEP    | \
			    ADLA_STS_HANG_MC_DEP    | \
			    ADLA_STS_HANG_DW_F_DEP  | \
			    ADLA_STS_HANG_DW_W_DEP  | \
			    ADLA_STS_HANG_RS_DEP)

/* POWER & CLOCK - 0x20..0x2C */
#define REG_ADLA_SWRST			0x00000020
#define ADLA_SWRST_ASSERT		BIT(0)

#define REG_ADLA_ENABLE			0x00000024
#define ADLA_ENABLE_BIT			BIT(0)

#define REG_ADLA_CLK_AUTOCLK		0x00000028
#define REG_ADLA_CLK_IDLE_CNT		0x0000002C
#define ADLA_CLK_AUTOCLK_EN		0x0000FFFF
#define ADLA_CLK_MODULE_EN		0x000000DF
#define ADLA_CLK_IDLE_CNT_DEFAULT	0x00000508

/* PARSER - 0x50..0xBC */
#define REG_ADLA_PS_CTRL		0x00000050
#define ADLA_PS_CTRL_RST		BIT(0)
#define ADLA_PS_CTRL_DEP_RST		BIT(1)
#define ADLA_PS_CTRL_START		BIT(2)
#define ADLA_PS_CTRL_PREEMPT		BIT(3)
#define ADLA_PS_CTRL_PEND_RST		BIT(4)

#define REG_ADLA_PS_STS			0x00000054
#define ADLA_PS_STS_STOP_CMD		BIT(0)
#define ADLA_PS_STS_STOP_ERR		BIT(1)
#define ADLA_PS_STS_STOP_PMT		BIT(2)
#define ADLA_PS_STS_BUSY		BIT(3)
#define ADLA_PS_STS_PMT_BUSY		BIT(4)

#define REG_ADLA_PS_RBF_BASE		0x00000064
#define REG_ADLA_PS_RBF_SIZE		0x00000068
#define REG_ADLA_PS_RBF_WPT		0x0000006C
#define REG_ADLA_PS_RBF_RPT		0x00000070

#define REG_ADLA_PS_PEND_EN		0x00000084
#define REG_ADLA_PS_PEND_TIMER		0x00000088
#define ADLA_PS_PEND_TIMER_EN		BIT(0)

/* SMMU - 0xC0..0xE4 */
#define REG_ADLA_SMMU_EN		0x000000C0
#define ADLA_SMMU_EN_BIT		BIT(0)
#define ADLA_SMMU_SWRST			BIT(1)

#define REG_ADLA_SMMU_TTBR_L		0x000000C4
#define REG_ADLA_SMMU_TTBR_H		0x000000C8

/* PM - 0xF0..0x108 */
#define REG_ADLA_PM_EN			0x000000F0
#define ADLA_PM_EN_HALF			BIT(0)
#define ADLA_PM_EN_FULL			BIT(1)
#define ADLA_PM_SWRST			BIT(2)

#define REG_ADLA_PM_RBF_BASE		0x000000F4
#define REG_ADLA_PM_RBF_SIZE		0x000000F8
#define REG_ADLA_PM_RBF_WPT		0x000000FC
#define REG_ADLA_PM_RBF_RPT		0x00000100

/* MC (Matrix Compute cluster) - 0x120..0x124 */
#define REG_ADLA_MC_CTL			0x00000120
#define ADLA_MC_CLK_GATE_EN		0x000000DF /* all subunits except bit5 */

#define REG_ADLA_MC_CLK_PHASE		0x00000124

struct adla_pm_domains {
	struct device **pd_dev;
	struct device_link **pd_dev_link;
	int num_domains;
};

struct adla_device {
	struct drm_device drm;
	struct adla_pm_domains pm;
	struct regulator *vdd;
	void __iomem *regs;
	struct clk *clk;
	struct clk *clk_sel;
	int irq;

	void *rbf_cpu;
	dma_addr_t rbf_dma;
	size_t rbf_size;
	u32 rbf_wpt;
	struct drm_gpu_scheduler sched;

	struct dma_fence *current_fence;
	spinlock_t fence_lock;
	u64 fence_ctx;
	u32 fence_seqno;

	struct xarray pending_jobs;
};

struct adla_file {
	struct drm_sched_entity entity;
};

#endif
