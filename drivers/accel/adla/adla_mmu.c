// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/scatterlist.h>
#include "adla_mmu.h"

static int adla_mmu_inv(struct adla_device *adla, u32 va, u32 ctl)
{
	u32 v;

	writel(va, adla->regs + REG_ADLA_SMMU_INV_VA);
	writel(ctl, adla->regs + REG_ADLA_SMMU_INV_CTL);
	return readl_poll_timeout(adla->regs + REG_ADLA_SMMU_INV_CTL, v,
				  !(v & ADLA_SMMU_INV_BUSY), 1, 1000);
}

int adla_mmu_ctx_activate(const struct adla_mmu_ctx *ctx)
{
	struct adla_device *adla = ctx->adla;
	const u64 root = (ctx->l1_dma >> 5) << 12;

	writel(ADLA_SMMU_EN_BIT, adla->regs + REG_ADLA_SMMU_EN);
	writel(lower_32_bits(root), adla->regs + REG_ADLA_SMMU_TTBR_L);
	writel(upper_32_bits(root), adla->regs + REG_ADLA_SMMU_TTBR_H);
	return adla_mmu_inv(adla, 0, ADLA_SMMU_INV_ALL);
}

static void adla_mmu_ctx_release(struct kref *ref);

struct adla_mmu_ctx *adla_mmu_ctx_create(struct adla_device *adla)
{
	struct device *dev = adla->drm.dev;
	struct adla_mmu_ctx *ctx;
	int i;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->adla = adla;
	kref_init(&ctx->ref);
	mutex_init(&ctx->lock);
	drm_mm_init(&ctx->mm, SZ_4K, ADLA_VA_SIZE - SZ_4K);

	ctx->l1 = dma_alloc_coherent(dev, SZ_4K, &ctx->l1_dma, GFP_KERNEL);
	if (!ctx->l1)
		goto err;

	for (i = 0; i < ADLA_VA_GB; i++) {
		ctx->l2[i] = dma_alloc_coherent(dev, SZ_4K, &ctx->l2_dma[i],
						GFP_KERNEL);
		if (!ctx->l2[i])
			goto err;
		ctx->l1[i] = ctx->l2_dma[i] | ADLA_PTE_TABLE_PAGE;
	}
	return ctx;
err:
	kref_put(&ctx->ref, adla_mmu_ctx_release);
	return ERR_PTR(-ENOMEM);
}

static void adla_mmu_ctx_release(struct kref *ref)
{
	struct adla_mmu_ctx *ctx = container_of(ref, struct adla_mmu_ctx, ref);
	struct device *dev = ctx->adla->drm.dev;
	int i, j;

	for (i = 0; i < ADLA_VA_GB; i++) {
		for (j = 0; j < ADLA_PT_ENTRIES; j++)
			if (ctx->l3[i][j])
				dma_free_coherent(dev, SZ_4K, ctx->l3[i][j],
						  ctx->l3_dma[i][j]);
		if (ctx->l2[i])
			dma_free_coherent(dev, SZ_4K, ctx->l2[i], ctx->l2_dma[i]);
	}
	if (ctx->l1)
		dma_free_coherent(dev, SZ_4K, ctx->l1, ctx->l1_dma);
	drm_mm_takedown(&ctx->mm);
	kfree(ctx);
}

void adla_mmu_ctx_get(struct adla_mmu_ctx *c) { kref_get(&c->ref); }

void adla_mmu_ctx_put(struct adla_mmu_ctx *c)
{
	kref_put(&c->ref, adla_mmu_ctx_release);
}

static int adla_mmu_map_page(struct adla_mmu_ctx *ctx, u64 iova, dma_addr_t pa)
{
	unsigned int i1 = L1_IDX(iova), i2 = L2_IDX(iova), i3 = L3_IDX(iova);
	u64 *l3;

	if (iova >= ADLA_VA_SIZE)
		return -EINVAL;

	l3 = ctx->l3[i1][i2];
	if (!l3) {
		l3 = dma_alloc_coherent(ctx->adla->drm.dev, SZ_4K,
					&ctx->l3_dma[i1][i2], GFP_KERNEL);
		if (!l3)
			return -ENOMEM;
		ctx->l3[i1][i2] = l3;
		ctx->l2[i1][i2] = ctx->l3_dma[i1][i2] | ADLA_PTE_TABLE_PAGE;
	}
	if (WARN_ON(l3[i3] & ADLA_PTE_VALID))
		return -EEXIST;

	l3[i3] = ((u64)pa & ADLA_PTE_ADDR_MASK) | ADLA_PTE_TABLE_PAGE;
	return 0;
}

static void adla_mmu_unmap_range(const struct adla_mmu_ctx *ctx, u64 iova, size_t size)
{
	for (; size; iova += SZ_4K, size -= SZ_4K) {
		u64 *l3 = ctx->l3[L1_IDX(iova)][L2_IDX(iova)];

		if (l3)
			l3[L3_IDX(iova)] = 0;
	}
}

int adla_mmu_map_sgt(struct adla_mmu_ctx *ctx, struct sg_table *sgt,
		     size_t size, bool high, struct drm_mm_node *node)
{
	struct scatterlist *sg;
	u64 iova;
	int i, ret;

	mutex_lock(&ctx->lock);

	ret = drm_mm_insert_node_in_range(&ctx->mm, node, size, SZ_4K, 0,
					  SZ_4K, ADLA_VA_SIZE,
					  high ? DRM_MM_INSERT_HIGH
					       : DRM_MM_INSERT_LOW);
	if (ret)
		goto out_unlock;

	iova = node->start;
	for_each_sgtable_dma_sg(sgt, sg, i) {
		dma_addr_t pa = sg_dma_address(sg);
		unsigned int len = sg_dma_len(sg);

		for (; len; len -= SZ_4K, pa += SZ_4K, iova += SZ_4K) {
			ret = adla_mmu_map_page(ctx, iova, pa);
			if (ret)
				goto err;
		}
	}

	dma_wmb();
	ctx->tlb_dirty = true;
	mutex_unlock(&ctx->lock);
	return 0;

err:
	adla_mmu_unmap_range(ctx, node->start, iova - node->start);
	drm_mm_remove_node(node);
out_unlock:
	mutex_unlock(&ctx->lock);
	return ret;
}

void adla_mmu_unmap(struct adla_mmu_ctx *ctx, struct drm_mm_node *node)
{
	mutex_lock(&ctx->lock);
	adla_mmu_unmap_range(ctx, node->start, node->size);;
	drm_mm_remove_node(node);
	dma_wmb();
	ctx->tlb_dirty = true;
	mutex_unlock(&ctx->lock);
}

int adla_mmu_map_contig(struct adla_mmu_ctx *ctx, u64 iova, dma_addr_t pa,
			size_t size)
{
	u64 start = iova;
	int ret = 0;

	mutex_lock(&ctx->lock);
	for (; size; iova += SZ_4K, pa += SZ_4K, size -= SZ_4K) {
		ret = adla_mmu_map_page(ctx, iova, pa);
		if (ret) {
			adla_mmu_unmap_range(ctx, start, iova - start);
			break;
		}
	}
	ctx->tlb_dirty = true;
	mutex_unlock(&ctx->lock);
	return ret;
}

int adla_mmu_prepare_job(struct adla_device *adla, struct adla_mmu_ctx *ctx)
{
	int ret = 0;

	mutex_lock(&ctx->lock);
	if (adla->active_ctx != ctx) {
		ret = adla_mmu_ctx_activate(ctx);
		if (!ret) {
			adla_mmu_ctx_get(ctx);
			if (adla->active_ctx)
				adla_mmu_ctx_put(adla->active_ctx);
			adla->active_ctx = ctx;
			ctx->tlb_dirty = false;
		}
	} else if (ctx->tlb_dirty) {
		ret = adla_mmu_inv(adla, 0, ADLA_SMMU_INV_ALL);
		if (!ret)
			ctx->tlb_dirty = false;
	}
	mutex_unlock(&ctx->lock);
	return ret;
}

void adla_mmu_reset(struct adla_device *adla)
{
	if (adla->active_ctx) {
		adla_mmu_ctx_put(adla->active_ctx);
		adla->active_ctx = NULL;
	}
}
