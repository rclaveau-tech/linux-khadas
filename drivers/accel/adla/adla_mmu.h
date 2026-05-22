// SPDX-License-Identifier: GPL-2.0-only

#ifndef __ADLA_MMU_H__
#define __ADLA_MMU_H__

#include <drm/drm_mm.h>
#include "adla_drv.h"

#define ADLA_PTE_VALID	BIT_ULL(0)
#define ADLA_PTE_TABLE_PAGE	(BIT_ULL(0) | BIT_ULL(1))
#define ADLA_PTE_ADDR_MASK	GENMASK_ULL(47, 12)

#define ADLA_VA_GB	2
#define ADLA_VA_SIZE	((u64)ADLA_VA_GB << 30)
#define ADLA_PT_ENTRIES	512

#define L1_IDX(a)	((unsigned int)((a) >> 30))
#define L2_IDX(a)	((unsigned int)(((a) >> 21) & 0x1ff))
#define L3_IDX(a)	((unsigned int)(((a) >> 12) & 0x1ff))

#define ADLA_RING_IOVA	0x7FF00000UL

struct adla_mmu_ctx {
	struct adla_device *adla;
	struct kref ref;

	struct mutex lock;
	struct drm_mm mm;
	bool tlb_dirty;

	u64 *l1;
	dma_addr_t l1_dma;
	u64 *l2[ADLA_VA_GB];
	dma_addr_t l2_dma[ADLA_VA_GB];
	u64 *l3[ADLA_VA_GB][ADLA_PT_ENTRIES];
	dma_addr_t l3_dma[ADLA_VA_GB][ADLA_PT_ENTRIES];
};

int adla_mmu_ctx_activate(const struct adla_mmu_ctx *ctx);
struct adla_mmu_ctx *adla_mmu_ctx_create(struct adla_device *adla);
void adla_mmu_ctx_get(struct adla_mmu_ctx *c);
void adla_mmu_ctx_put(struct adla_mmu_ctx *ctx);
int adla_mmu_map_sgt(struct adla_mmu_ctx *ctx, struct sg_table *sgt,
		     size_t size, bool high, struct drm_mm_node *node);
void adla_mmu_unmap(struct adla_mmu_ctx *ctx, struct drm_mm_node *node);
int adla_mmu_map_contig(struct adla_mmu_ctx *ctx, u64 iova, dma_addr_t pa,
			size_t size);
int adla_mmu_prepare_job(struct adla_device *adla, struct adla_mmu_ctx *ctx);
void adla_mmu_reset(struct adla_device *adla);

#endif //__ADLA_MMU_H__
