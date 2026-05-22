// SPDX-License-Identifier: GPL-2.0-only

#include <drm/drm_accel.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <uapi/drm/adla_accel.h>

#include "adla_drv.h"
#include "adla_job.h"

static int adla_attach_pm_domains(struct device *dev, struct adla_pm_domains *pm)
{
	int i, ret;

	pm->num_domains = of_count_phandle_with_args(dev->of_node, "power-domains",
						     "#power-domain-cells");
	if (pm->num_domains <= 1)
		return 0;

	pm->pd_dev = devm_kmalloc_array(dev, pm->num_domains,
					sizeof(*pm->pd_dev), GFP_KERNEL);
	if (!pm->pd_dev)
		return -ENOMEM;

	pm->pd_dev_link = devm_kmalloc_array(dev, pm->num_domains,
					     sizeof(*pm->pd_dev_link), GFP_KERNEL);
	if (!pm->pd_dev_link)
		return -ENOMEM;

	for (i = 0; i < pm->num_domains; i++) {
		pm->pd_dev[i] = dev_pm_domain_attach_by_id(dev, i);
		if (IS_ERR(pm->pd_dev[i])) {
			ret = PTR_ERR(pm->pd_dev[i]);
			goto detach;
		}

		pm->pd_dev_link[i] = device_link_add(dev, pm->pd_dev[i],
						     DL_FLAG_STATELESS |
						     DL_FLAG_PM_RUNTIME |
						     DL_FLAG_RPM_ACTIVE);
		if (!pm->pd_dev_link[i]) {
			dev_pm_domain_detach(pm->pd_dev[i], false);
			ret = -EINVAL;
			goto detach;
		}
	}
	return 0;

detach:
	while (--i >= 0) {
		device_link_del(pm->pd_dev_link[i]);
		dev_pm_domain_detach(pm->pd_dev[i], false);
	}
	return ret;
}

static irqreturn_t adla_irq_handler(int irq, void *data)
{
	struct adla_device *adla = data;
	u32 status;

	dev_info(adla->drm.dev, "IRQ fired\n");

	status = readl(adla->regs + REG_ADLA_IRQ_MASKED);
	if (!status)
		return IRQ_NONE;

	dev_info(adla->drm.dev, "IRQ status: 0x%08x\n", status);

	writel(status, adla->regs + REG_ADLA_IRQ_RAW);
	(void)readl(adla->regs + REG_ADLA_IRQ_MASKED);

	if ((status & ADLA_IRQ_TIME_STAMP) || (status & ADLA_IRQ_PARSER_STOP_CMD)) {
		spin_lock(&adla->fence_lock);
		if (adla->current_fence) {
			dma_fence_signal(adla->current_fence);
			dma_fence_put(adla->current_fence);
			adla->current_fence = NULL;
		}
		spin_unlock(&adla->fence_lock);
	}

	if (status & ADLA_IRQ_PARSER_STOP_ERR)
		dev_err(adla->drm.dev, "parser error!\n");

	if (status & ADLA_IRQ_INVALID_IOVA)
		dev_err(adla->drm.dev, "SMMU invalid IOVA!\n");

	return IRQ_HANDLED;
}

static int adla_hw_init(struct device *dev, struct adla_device *adla)
{
	u32 major, minor, rev, sts;

	rev = readl(adla->regs + REG_ADLA_REVISION);
	if (rev == 0xFFFFFFFF) {
		dev_err(adla->drm.dev, "failed to read REV register (bus error?)\n");
		return -EIO;
	}
	major = (rev & ADLA_REVISION_MAJOR__MASK) >> ADLA_REVISION_MAJOR__SHIFT;
	minor = (rev & ADLA_REVISION_MINOR__MASK) >> ADLA_REVISION_MINOR__SHIFT;
	dev_info(adla->drm.dev, "ADLA HW rev %d.%d\n", major, minor);

	writel(ADLA_SWRST_ASSERT, adla->regs + REG_ADLA_SWRST);
	udelay(10);
	writel(0, adla->regs + REG_ADLA_SWRST);
	udelay(10);

	/*writel(0, adla->regs + REG_ADLA_IRQ_MASK);
	writel(0, adla->regs + REG_ADLA_IRQ_RAW);*/

	writel(ADLA_ENABLE_BIT, adla->regs + REG_ADLA_ENABLE);

	writel(ADLA_CLK_AUTOCLK_EN, adla->regs + REG_ADLA_CLK_AUTOCLK);
	writel(ADLA_MC_CLK_GATE_EN, adla->regs + REG_ADLA_MC_CTL);
	writel(ADLA_CLK_IDLE_CNT_DEFAULT, adla->regs + REG_ADLA_CLK_IDLE_CNT);

	writel(0, adla->regs + REG_ADLA_SMMU_EN);

	writel(0xFF, adla->regs + REG_ADLA_PS_APB_TIMEOUT);
	writel(ADLA_PS_PEND_TIMER_EN, adla->regs + REG_ADLA_PS_PEND_EN);
	writel(0x1000000, adla->regs + REG_ADLA_PS_PEND_TIMER);

	sts = readl(adla->regs + REG_ADLA_STS_REPORT);
	if (sts & ADLA_STS_HANG_MASK) {
		dev_warn(adla->drm.dev, "STS_REPORT not empty after reset: 0x%08x\n", sts);
	}

	adla->rbf_size = SZ_64K;
	adla->rbf_cpu = dmam_alloc_coherent(dev, adla->rbf_size,
					    &adla->rbf_dma, GFP_KERNEL);
	if (!adla->rbf_cpu)
		return -ENOMEM;

	adla->rbf_wpt = 0;
	writel(lower_32_bits(adla->rbf_dma), adla->regs + REG_ADLA_PS_RBF_BASE);
	writel(adla->rbf_size, adla->regs + REG_ADLA_PS_RBF_SIZE);
	writel(0, adla->regs + REG_ADLA_PS_RBF_WPT);
	writel(0, adla->regs + REG_ADLA_PS_RBF_RPT);

	writel(0xFFFF, adla->regs + REG_ADLA_IRQ_RAW);
	/*writel(ADLA_IRQ_PARSER_STOP_CMD |
	       ADLA_IRQ_PARSER_STOP_ERR |
	       ADLA_IRQ_INVALID_IOVA |
	       ADLA_IRQ_TIME_STAMP,
	       adla->regs + REG_ADLA_IRQ_MASK);*/
	writel(0xFFFF, adla->regs + REG_ADLA_IRQ_MASK);

	writel(ADLA_PS_CTRL_START, adla->regs + REG_ADLA_PS_CTRL);

	return 0;
}

static int adla_drm_open(struct drm_device *dev, struct drm_file *filp)
{
	struct adla_device *adla = container_of(dev, struct adla_device, drm);
	struct drm_gpu_scheduler *scheds[] = { &adla->sched };
	struct adla_file *afile;
	int ret;

	afile = kzalloc(sizeof(*afile), GFP_KERNEL);
	if (!afile)
		return -ENOMEM;

	ret = drm_sched_entity_init(&afile->entity,
				    DRM_SCHED_PRIORITY_NORMAL,
				    scheds, ARRAY_SIZE(scheds), NULL);
	if (ret) {
		kfree(afile);
		return ret;
	}

	filp->driver_priv = afile;
	return 0;
}

static void adla_drm_postclose(struct drm_device *dev, struct drm_file *filp)
{
	struct adla_file *afile = filp->driver_priv;

	drm_sched_entity_destroy(&afile->entity);
	kfree(afile);
}

static const struct drm_ioctl_desc adla_drm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(ADLA_SUBMIT, adla_ioctl_submit, 0),
	DRM_IOCTL_DEF_DRV(ADLA_WAIT, adla_ioctl_wait, 0),
};

DEFINE_DRM_ACCEL_FOPS(adla_fops);

static const struct drm_driver adla_drm_driver = {
	.driver_features	= DRIVER_COMPUTE_ACCEL,
	.fops			= &adla_fops,
	.open			= adla_drm_open,
	.postclose		= adla_drm_postclose,
	.ioctls			= adla_drm_ioctls,
	.num_ioctls		= ARRAY_SIZE(adla_drm_ioctls),
	.name			= "adla",
	.desc			= "ADLA DRM",
};

static int adla_probe(struct platform_device *pdev)
{
	struct adla_device *adla;
	int ret;

	adla = devm_drm_dev_alloc(&pdev->dev, &adla_drm_driver,
				  struct adla_device, drm);
	if (IS_ERR(adla))
		return PTR_ERR(adla);

	platform_set_drvdata(pdev, adla);

	adla->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(adla->regs))
		return PTR_ERR(adla->regs);

	ret = adla_attach_pm_domains(&pdev->dev, &adla->pm);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to attach power domains\n");

	ret = devm_regulator_get_enable(&pdev->dev, "vnpu");
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to get vnpu supply\n");

	adla->clk = devm_clk_get_enabled(&pdev->dev, "adla_core_clk");
	if (IS_ERR(adla->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(adla->clk),
				     "failed to get adla_core_clk\n");

	adla->clk_sel = devm_clk_get_enabled(&pdev->dev, "adla_clk");
	if (IS_ERR(adla->clk_sel))
		return dev_err_probe(&pdev->dev, PTR_ERR(adla->clk_sel),
				     "failed to get adla_clk\n");

	adla->irq = platform_get_irq(pdev, 0);
	if (adla->irq < 0)
		return adla->irq;

	ret = adla_hw_init(&pdev->dev, adla);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "HW init failed\n");

	ret = devm_request_irq(&pdev->dev, adla->irq, adla_irq_handler,
			       IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), adla);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to request IRQ\n");

	ret = adla_job_init(&pdev->dev, adla);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "failed to init job scheduler\n");

	ret = drm_dev_register(&adla->drm, 0);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to register DRM device\n");

	return 0;
}

static void adla_remove(struct platform_device *pdev)
{
	struct adla_device *adla = dev_get_drvdata(&pdev->dev);
	int i;

	if (!adla)
		return;

	for (i = 0; i < adla->pm.num_domains; i++) {
		device_link_del(adla->pm.pd_dev_link[i]);
		dev_pm_domain_detach(adla->pm.pd_dev[i], false);
	}

	drm_dev_unplug(&adla->drm);
	adla_job_exit();
}

static const struct of_device_id adla_of_match[] = {
	{ .compatible = "amlogic,t7-adla", },
	{}
};
MODULE_DEVICE_TABLE(of, adla_of_match);

static struct platform_driver adla_drv = {
	.probe	= adla_probe,
	.remove	= adla_remove,
	.driver	= {
		.name = "adla",
		.of_match_table = adla_of_match,
	},
};

module_platform_driver(adla_drv);

MODULE_AUTHOR("Ronald Claveau <linux-kernel-dev@aliel.fr>");
MODULE_DESCRIPTION("Amlogic ADLA NPU driver");
MODULE_LICENSE("GPL");
