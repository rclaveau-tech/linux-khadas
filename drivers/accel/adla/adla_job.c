// SPDX-License-Identifier: GPL-2.0-only

#include <drm/drm_file.h>
#include <linux/delay.h>
#include <uapi/drm/adla_accel.h>

#include "adla_drv.h"
#include "adla_job.h"

static struct kmem_cache *adla_fence_cache;

static const char *adla_fence_get_driver_name(struct dma_fence *fence)
{
	return "adla";
}

static const char *adla_fence_get_timeline_name(struct dma_fence *fence)
{
	return "adla-npu";
}

static void adla_fence_release(struct dma_fence *fence)
{
	kmem_cache_free(adla_fence_cache, fence);
}

static const struct dma_fence_ops adla_fence_ops = {
	.get_driver_name	= adla_fence_get_driver_name,
	.get_timeline_name	= adla_fence_get_timeline_name,
	.release		= adla_fence_release,
};

static void adla_rb_write(struct adla_device *adla, u32 cmd)
{
	u32 *rb = adla->rbf_cpu;
	u32 idx = adla->rbf_wpt / sizeof(u32);

	WARN_ON(idx >= adla->rbf_size / sizeof(u32));

	rb[idx] = cmd;
	adla->rbf_wpt += sizeof(u32);
	adla->rbf_wpt %= adla->rbf_size;
}

static void adla_rb_align(struct adla_device *adla)
{
	while (adla->rbf_wpt % ADLA_CMD_ALIGN)
		adla_rb_write(adla, ADLA_CMD_NOP);
}

static void adla_rb_flush(struct adla_device *adla)
{
	adla_rb_align(adla);
	dev_info(adla->drm.dev, "flush: wpt=0x%x\n", adla->rbf_wpt);

	dev_info(adla->drm.dev, "pre-flush: CLK_AUTOCLK=0x%x MC_CTL=0x%x\n",
		readl(adla->regs + REG_ADLA_CLK_AUTOCLK),
		readl(adla->regs + REG_ADLA_MC_CTL));
	dev_info(adla->drm.dev, "pre-flush: ENABLE=0x%x SMMU_EN=0x%x\n",
		readl(adla->regs + REG_ADLA_ENABLE),
		readl(adla->regs + REG_ADLA_SMMU_EN));
	dev_info(adla->drm.dev, "pre-flush: RBF_BASE=0x%x RBF_SIZE=0x%x\n",
		readl(adla->regs + REG_ADLA_PS_RBF_BASE),
		readl(adla->regs + REG_ADLA_PS_RBF_SIZE));
	dev_info(adla->drm.dev, "pre-flush: PS_STS=0x%x RPT=0x%x WPT=0x%x\n",
		readl(adla->regs + REG_ADLA_PS_STS),
		readl(adla->regs + REG_ADLA_PS_RBF_RPT),
		readl(adla->regs + REG_ADLA_PS_RBF_WPT));
	dev_info(adla->drm.dev, "ring buffer dump (%u bytes):\n", adla->rbf_wpt);
	for (u32 i = 0; i < adla->rbf_wpt / 4; i++)
		dev_info(adla->drm.dev, "  [%02x] 0x%08x\n",
			 i * 4, ((u32 *)adla->rbf_cpu)[i]);

	writel(adla->rbf_wpt & 0x0FFFFFFF, adla->regs + REG_ADLA_PS_RBF_WPT);
	dev_info(adla->drm.dev, "after WPT: PS_STS=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_STS));
	dev_info(adla->drm.dev, "PS_STS=0x%x RPT=0x%x IRQ_RAW=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_STS),
		 readl(adla->regs + REG_ADLA_PS_RBF_RPT),
		 readl(adla->regs + REG_ADLA_IRQ_RAW));
	//writel(ADLA_PS_CTRL_START, adla->regs + REG_ADLA_PS_CTRL);
	for (int i = 0; i < 20; i++) {
		udelay(100);
		dev_info(adla->drm.dev, "PS_STS[%d]=0x%x RPT=0x%x IRQ_RAW=0x%x\n",
			 i,
			 readl(adla->regs + REG_ADLA_PS_STS),
			 readl(adla->regs + REG_ADLA_PS_RBF_RPT),
			 readl(adla->regs + REG_ADLA_IRQ_RAW));
	}
	dev_info(adla->drm.dev, "after START: PS_STS=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_STS));

	udelay(10);
	dev_info(adla->drm.dev, "PS_CTRL=0x%x PS_STS=0x%x RPT=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_CTRL),
		 readl(adla->regs + REG_ADLA_PS_STS),
		 readl(adla->regs + REG_ADLA_PS_RBF_RPT));

	udelay(100);
	dev_info(adla->drm.dev, "PS_STS=0x%x RPT=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_STS),
		 readl(adla->regs + REG_ADLA_PS_RBF_RPT));
	dev_info(adla->drm.dev, "post-flush: PS_STS=0x%x PS_CTRL=0x%x\n",
		readl(adla->regs + REG_ADLA_PS_STS),
		readl(adla->regs + REG_ADLA_PS_CTRL));
}

static struct dma_fence *adla_run_job(struct drm_sched_job *sched_job)
{
	struct adla_job *job  = container_of(sched_job, struct adla_job, base);
	struct adla_device *adla = job->adla;
	struct dma_fence *fence;
	u32 config_wpt, off;

	job->hw_fence = kmem_cache_zalloc(adla_fence_cache, GFP_KERNEL);
	dma_fence_init(job->hw_fence, &adla_fence_ops, &adla->fence_lock,
		       adla->fence_ctx, ++adla->fence_seqno);

	spin_lock(&adla->fence_lock);
	adla->current_fence = dma_fence_get(job->hw_fence);
	spin_unlock(&adla->fence_lock);

	adla_rb_write(adla, ADLA_CMD_SET_FENCE | 0x700000);
	adla_rb_write(adla, ADLA_CMD_RESET_ID);
	adla_rb_align(adla);

	adla_rb_write(adla, ADLA_CMD_SET_SW_ID | (job->id & ADLA_SW_ID_MASK));
	adla_rb_write(adla, ADLA_CMD_SET_DEPENDENCY | job->dep);
	adla_rb_write(adla, ADLA_CMD_EXECUTE        | job->output_modules);
	adla_rb_write(adla, ADLA_CMD_CONFIG         | job->active_modules);

	config_wpt = adla->rbf_wpt;

	for (int i = 0; i < job->config_size / sizeof(u32); i++)
		adla_rb_write(adla, job->config[i]);

	for (u32 i = 0; i < job->fixup_count; i++) {
		struct adla_fixup *f = &job->fixups[i];
		off = (config_wpt + f->blob_offset) % adla->rbf_size;
		*((u32 *)(adla->rbf_cpu + off)) = lower_32_bits(f->iova);
	}

	adla_rb_write(adla, ADLA_CMD_SET_FENCE | job->fence_modules);
	adla_rb_align(adla);
	adla_rb_write(adla, ADLA_CMD_SET_TIME_STAMP | ADLA_TIME_STAMP_IRQ);
	adla_rb_write(adla, job->id);

	fence = dma_fence_get(job->hw_fence);

	adla_rb_flush(adla);
	dev_info(adla->drm.dev, "job %u: wpt=0x%x rbf_base=0x%llx\n",
	 job->id, adla->rbf_wpt, (u64)adla->rbf_dma);
	dev_info(adla->drm.dev, "HW wpt=0x%x rpt=0x%x\n",
		 readl(adla->regs + REG_ADLA_PS_RBF_WPT),
		 readl(adla->regs + REG_ADLA_PS_RBF_RPT));
	dev_info(adla->drm.dev, "IRQ_MASK=0x%x IRQ_RAW=0x%x IRQ_MASKED=0x%x\n",
		 readl(adla->regs + REG_ADLA_IRQ_MASK),
		 readl(adla->regs + REG_ADLA_IRQ_RAW),
		 readl(adla->regs + REG_ADLA_IRQ_MASKED));

	return fence;
}

static enum drm_gpu_sched_stat adla_timedout_job(struct drm_sched_job *sched_job)
{
	struct adla_job *job  = container_of(sched_job, struct adla_job, base);
	struct adla_device *adla = job->adla;
	//u32 sts;

	dev_warn(adla->drm.dev, "timeout: IRQ_RAW=0x%x IRQ_MASKED=0x%x IRQ_MASK=0x%x PS_STS=0x%x TIME_STAMP=0x%x\n",
		 readl(adla->regs + REG_ADLA_IRQ_RAW),
		 readl(adla->regs + REG_ADLA_IRQ_MASKED),
		 readl(adla->regs + REG_ADLA_IRQ_MASK),
		 readl(adla->regs + REG_ADLA_PS_STS),
		 readl(adla->regs + 0x60));

	/*sts = readl(adla->regs + REG_ADLA_PS_STS);
	if (!(sts & ADLA_PS_STS_BUSY)) {
		dev_warn(adla->drm.dev, "job %u: timed out but idle parser\n", job->id);
		return DRM_GPU_SCHED_STAT_NO_HANG;
	}*/

	dev_err(adla->drm.dev, "job %u timed out, STS_REPORT=0x%08x\n",
		job->id, readl(adla->regs + REG_ADLA_STS_REPORT));

	/* Reset hardware */
	writel(0, adla->regs + REG_ADLA_IRQ_MASK);
	writel(ADLA_SWRST_ASSERT, adla->regs + REG_ADLA_SWRST);
	udelay(10);
	writel(0, adla->regs + REG_ADLA_SWRST);

	/* Ring buffer reinit */
	adla->rbf_wpt = 0;
	writel(0, adla->regs + REG_ADLA_PS_RBF_WPT);
	writel(0, adla->regs + REG_ADLA_PS_RBF_RPT);

	if (readl(adla->regs + REG_ADLA_REVISION) == 0xFFFFFFFF) {
		dev_err(adla->drm.dev, "hardware unreachable after reset\n");
		return DRM_GPU_SCHED_STAT_ENODEV;
	}

	writel(ADLA_ENABLE_BIT, adla->regs + REG_ADLA_ENABLE);
	writel(ADLA_IRQ_PARSER_STOP_CMD |
	       ADLA_IRQ_PARSER_STOP_ERR |
	       ADLA_IRQ_INVALID_IOVA,
	       adla->regs + REG_ADLA_IRQ_MASK);

	return DRM_GPU_SCHED_STAT_RESET;
}

static void adla_free_job(struct drm_sched_job *sched_job)
{
	struct adla_job *job = container_of(sched_job, struct adla_job, base);

	drm_sched_job_cleanup(&job->base);
	dma_fence_put(job->done_fence);
	dma_fence_put(job->hw_fence);
	kfree(job->fixups);
	kfree(job->config);
	kfree(job);
}

static const struct drm_sched_backend_ops adla_sched_ops = {
	.run_job	= adla_run_job,
	.timedout_job	= adla_timedout_job,
	.free_job	= adla_free_job,
};

int adla_ioctl_submit(struct drm_device *dev, void *data, struct drm_file *filp) {
	struct drm_adla_submit *submit = data;
	struct adla_device *adla = container_of(dev, struct adla_device, drm);
	struct adla_file *afile = filp->driver_priv;
	struct adla_job *job;
	int ret = 0;
	u32 id;

	if (/*submit->config_size == 0 || */submit->config_size > SZ_1M)
		return -EINVAL;

	job = kzalloc(sizeof (*job), GFP_KERNEL);
	if (!job)
		return -ENOMEM;

	job->adla = adla;
	job->output_modules = submit->output_modules;
	job->active_modules = submit->active_modules;
	job->fence_modules = submit->fence_modules;
	job->dep = submit->dep;

	job->config_size = submit->config_size;
	job->config = kmalloc(submit->config_size, GFP_KERNEL);
	if (!job->config) { ret = -ENOMEM; goto err_free_job; }

	if (copy_from_user(job->config, u64_to_user_ptr(submit->config_ptr),
			   submit->config_size)) {
		ret = -EFAULT;
		goto err_free_config;
	}

	job->fixup_count = submit->fixup_count;
	if (job->fixup_count) {
		job->fixups = kmalloc_array(job->fixup_count,
					    sizeof(struct adla_fixup), GFP_KERNEL);
		if (!job->fixups) { ret = -ENOMEM; goto err_free_config; }

		if (copy_from_user(job->fixups, u64_to_user_ptr(submit->fixup_ptr),
				   job->fixup_count * sizeof(struct adla_fixup))) {
			ret = -EFAULT;
			goto err_free_fixups;
		}
	}

	ret = xa_alloc(&adla->pending_jobs, &id, job, xa_limit_32b, GFP_KERNEL);
	if (ret) {
		dev_err(adla->drm.dev, "xa_alloc failed: %d\n", ret);
		goto err_free_fixups;
	}

	job->id = id;

	ret = drm_sched_job_init(&job->base, &afile->entity, 1, filp,
				 filp->client_id);
	if (ret) {
		dev_err(adla->drm.dev, "drm_sched_job_init failed: %d\n", ret);
		goto err_xa_erase;
	}

	drm_sched_job_arm(&job->base);
	job->done_fence = dma_fence_get(&job->base.s_fence->finished);
	xa_store(&adla->pending_jobs, id, job->done_fence, GFP_KERNEL);
	drm_sched_entity_push_job(&job->base);

	submit->out_job_id = id;

	return 0;

err_xa_erase:
	xa_erase(&adla->pending_jobs, id);
err_free_fixups:
	kfree(job->fixups);
err_free_config:
	kfree(job->config);
err_free_job:
	kfree(job);
	return ret;
}

int adla_ioctl_wait(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_adla_wait *args = data;
	struct adla_device *adla = container_of(dev, struct adla_device, drm);
	struct dma_fence *fence;
	long ret;

	fence = xa_erase(&adla->pending_jobs, args->job_id);
	if (!fence)
		return -ENOENT;

	ret = dma_fence_wait_timeout(fence, true,
		args->timeout_ns < 0 ? MAX_SCHEDULE_TIMEOUT : nsecs_to_jiffies(args->timeout_ns));
	dma_fence_put(fence);

	if (ret == 0)  return -ETIMEDOUT;
	if (ret < 0)   return ret;
	return 0;
}

int adla_job_init(struct device *dev, struct adla_device *adla)
{
	int ret;

	xa_init_flags(&adla->pending_jobs, XA_FLAGS_ALLOC);

	const struct drm_sched_init_args sched_args = {
		.ops		= &adla_sched_ops,
		.credit_limit	= 1,
		.timeout	= msecs_to_jiffies(500),
		.name		= "adla",
		.dev		= adla->drm.dev,
	};

	adla_fence_cache = KMEM_CACHE(dma_fence, SLAB_HWCACHE_ALIGN);
	spin_lock_init(&adla->fence_lock);
	adla->fence_ctx = dma_fence_context_alloc(1);

	ret = drm_sched_init(&adla->sched, &sched_args);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init scheduler\n");

	return 0;
}

void adla_job_exit(void)
{
	kmem_cache_destroy(adla_fence_cache);
}
