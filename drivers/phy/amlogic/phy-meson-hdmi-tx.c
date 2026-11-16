// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Amlogic Meson HDMI TX PHY driver (GXBB / GXL / GXM / G12A)
 *
 * Copyright (C) 2026 Ronald Claveau <linux-kernel-dev@aliel.fr>
 *
 * Based on meson_dw_hdmi.c:
 * Copyright (C) 2016 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/phy/phy-hdmi.h>
#include <linux/phy/phy-meson-hdmi-tx.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define HHI_HDMI_PHY_CNTL0	0x3a0 /* 0xe8 */
#define HHI_HDMI_PHY_CNTL1	0x3a4 /* 0xe9 */
#define  PHY_CNTL1_INIT		0x03900000
#define  PHY_INVERT		BIT(17)
#define  PHY_CNTL1_CTRL_MASK	GENMASK(3, 0)
#define  PHY_CNTL1_RESET	0xf /* enable + software reset */
#define  PHY_CNTL1_UNRESET	0xe /* enable + unreset */
#define HHI_HDMI_PHY_CNTL2	0x3a8 /* 0xea */
#define HHI_HDMI_PHY_CNTL3	0x3ac /* 0xeb */
#define HHI_HDMI_PHY_CNTL4	0x3b0 /* 0xec */
#define HHI_HDMI_PHY_CNTL5	0x3b4 /* 0xed */

/* The code resets the PHY 3 times in a row */
#define PHY_RESET_COUNT		3

struct meson_hdmi_phy_rate {
	unsigned int min_rate;	/* TMDS character rate, in Hz */
	u32 cntl0;
	u32 cntl3;
	u32 cntl5;		/* only used if has_cntl5 */
};

struct meson_hdmi_phy_data {
	const struct meson_hdmi_phy_rate *rates;
	unsigned int num_rates;
	unsigned int max_rate;	/* Hz, TMDS character rate maximum */
	bool has_cntl5;
	u32 cntl0_init;
	u32 cntl1_init;
};

struct meson_hdmi_phy {
	struct regmap *hhi;
	const struct meson_hdmi_phy_data *data;
};

/* GXBB */
static const struct meson_hdmi_phy_rate gxbb_rates[] = {
	/* 5.94Gbps, 3.7125Gbps */
	{ 371250000, 0x33353245, 0x2100115b },
	/* 2.97Gbps */
	{ 297000000, 0x33634283, 0xb000115b },
	/* 1.485Gbps, down to 251.75Mbps */
	{ 25175000,  0x33632122, 0x2000115b },
};

/* GXL / GXM */
static const struct meson_hdmi_phy_rate gxl_rates[] = {
	/* 5.94Gbps, 3.7125Gbps */
	{ 371250000, 0x333d3282, 0x2136315b },
	/* 2.97Gbps */
	{ 297000000, 0x33303382, 0x2036315b },
	/* 1.485Gbps */
	{ 148500000, 0x33303362, 0x2016315b },
	/* 742.5Mbps, down to 251.75Mbps */
	{ 25175000,  0x33604142, 0x0016315b },
};

/* G12A */
static const struct meson_hdmi_phy_rate g12a_rates[] = {
	/* 5.94Gbps, 3.7125Gbps */
	{ 371250000, 0x37eb65c4, 0x2ab0ff3b, 0x0000080b },
	/* 2.97Gbps */
	{ 297000000, 0x33eb6262, 0x2ab0ff3b, 0x00000003 },
	/* 1.485Gbps, down to 251.75Mbps */
	{ 25175000,  0x33eb4242, 0x2ab0ff3b, 0x00000003 },
};

static const struct meson_hdmi_phy_data gxbb_data = {
	.rates = gxbb_rates,
	.num_rates = ARRAY_SIZE(gxbb_rates),
	.max_rate  = 600000000, /* HDMI 2.0 */
	.cntl0_init = 0x0,
	.cntl1_init = PHY_CNTL1_INIT | PHY_INVERT,
};

static const struct meson_hdmi_phy_data gxl_data = {
	.rates = gxl_rates,
	.num_rates = ARRAY_SIZE(gxl_rates),
	.max_rate  = 600000000, /* HDMI 2.0 */
	.cntl0_init = 0x0,
	.cntl1_init = PHY_CNTL1_INIT,
};

static const struct meson_hdmi_phy_data g12a_data = {
	.rates = g12a_rates,
	.num_rates = ARRAY_SIZE(g12a_rates),
	.max_rate  = 600000000, /* HDMI 2.0 */
	.has_cntl5 = true,
	.cntl0_init = 0x000b4242, /* Bandgap */
	.cntl1_init = PHY_CNTL1_INIT,
};

static const struct meson_hdmi_phy_rate *
meson_hdmi_phy_find_rate(const struct meson_hdmi_phy_data *data, u64 rate)
{
	unsigned int i;

	if (rate > data->max_rate)
		return NULL;

	for (i = 0; i < data->num_rates; i++)
		if (rate >= data->rates[i].min_rate)
			return &data->rates[i];

	return NULL;
}

/* Fallback to init mode: CNTL1 first, then CNTL0 (order matters) */
static void meson_hdmi_phy_set_init_state(struct meson_hdmi_phy *priv)
{
	regmap_write(priv->hhi, HHI_HDMI_PHY_CNTL1, priv->data->cntl1_init);
	regmap_write(priv->hhi, HHI_HDMI_PHY_CNTL0, priv->data->cntl0_init);
}

static int meson_hdmi_phy_init(struct phy *phy)
{
	struct meson_hdmi_phy *priv = phy_get_drvdata(phy);

	meson_hdmi_phy_set_init_state(priv);

	return 0;
}

static int meson_hdmi_phy_configure(struct phy *phy,
				    union phy_configure_opts *opts)
{
	struct meson_hdmi_phy *priv = phy_get_drvdata(phy);
	const struct meson_hdmi_phy_rate *r;

	r = meson_hdmi_phy_find_rate(priv->data, opts->hdmi.tmds_char_rate);
	if (!r)
		return -EINVAL;

	regmap_write(priv->hhi, HHI_HDMI_PHY_CNTL0, r->cntl0);
	regmap_write(priv->hhi, HHI_HDMI_PHY_CNTL3, r->cntl3);
	if (priv->data->has_cntl5)
		regmap_write(priv->hhi, HHI_HDMI_PHY_CNTL5, r->cntl5);

	/* Disable clock, fifo, fifo_wr */
	regmap_update_bits(priv->hhi, HHI_HDMI_PHY_CNTL1,
			   PHY_CNTL1_CTRL_MASK, 0);

	return 0;
}

static int meson_hdmi_phy_validate(struct phy *phy, enum phy_mode mode,
				   int submode, union phy_configure_opts *opts)
{
	struct meson_hdmi_phy *priv = phy_get_drvdata(phy);

	if (mode != PHY_MODE_HDMI)
		return -EINVAL;

	if (submode != PHY_HDMI_MODE_TMDS)
		return -EINVAL;

	if (opts->hdmi.bpc != 8)
		return -EINVAL;

	if (!meson_hdmi_phy_find_rate(priv->data, opts->hdmi.tmds_char_rate))
		return -EINVAL;

	return 0;
}

static int meson_hdmi_phy_power_on(struct phy *phy)
{
	struct meson_hdmi_phy *priv = phy_get_drvdata(phy);
	int i;

	/* Reset PHY 3 times in a row */
	for (i = 0; i < PHY_RESET_COUNT; i++) {
		/* Enable and software reset */
		regmap_update_bits(priv->hhi, HHI_HDMI_PHY_CNTL1,
				   PHY_CNTL1_CTRL_MASK, PHY_CNTL1_RESET);
		fsleep(2000);

		/* Enable and unreset */
		regmap_update_bits(priv->hhi, HHI_HDMI_PHY_CNTL1,
				   PHY_CNTL1_CTRL_MASK, PHY_CNTL1_UNRESET);
		fsleep(2000);
	}

	return 0;
}

static int meson_hdmi_phy_power_off(struct phy *phy)
{
	struct meson_hdmi_phy *priv = phy_get_drvdata(phy);

	meson_hdmi_phy_set_init_state(priv);

	return 0;
}

static const struct phy_ops meson_hdmi_phy_ops = {
	.init		= meson_hdmi_phy_init,
	.configure	= meson_hdmi_phy_configure,
	.validate	= meson_hdmi_phy_validate,
	.power_on	= meson_hdmi_phy_power_on,
	.power_off	= meson_hdmi_phy_power_off,
	.owner		= THIS_MODULE,
};

static struct phy *meson_hdmi_phy_alloc(struct device *dev,
					struct regmap *hhi,
					const struct meson_hdmi_phy_data *data)
{
	struct meson_hdmi_phy *priv;
	struct phy *phy;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	priv->hhi = hhi;
	priv->data = data;

	phy = devm_phy_create(dev, NULL, &meson_hdmi_phy_ops);
	if (IS_ERR(phy))
		return phy;

	phy_set_drvdata(phy, priv);

	return phy;
}

/*
 * DEPRECATED: only for DTs without a "phys" property on the HDMI TX node.
 * Remove together with the fallback in meson_dw_hdmi.c.
 */
struct phy *meson_hdmi_phy_create_legacy(struct device *dev,
					 struct regmap *hhi,
					 enum meson_hdmi_phy_soc soc)
{
	static const struct meson_hdmi_phy_data *const soc_data[] = {
		[MESON_HDMI_PHY_GXBB] = &gxbb_data,
		[MESON_HDMI_PHY_GXL]  = &gxl_data,
		[MESON_HDMI_PHY_G12A] = &g12a_data,
	};

	if (soc >= ARRAY_SIZE(soc_data))
		return ERR_PTR(-EINVAL);

	return meson_hdmi_phy_alloc(dev, hhi, soc_data[soc]);
}
EXPORT_SYMBOL_GPL(meson_hdmi_phy_create_legacy);

static int meson_hdmi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct regmap *hhi;
	struct phy *phy;
	const struct meson_hdmi_phy_data *data;

	data = of_device_get_match_data(dev);
	if (!data)
		return -EINVAL;

	/* The PHY node is a child of the HHI syscon */
	hhi = syscon_node_to_regmap(dev->of_node->parent);
	if (IS_ERR(hhi))
		return dev_err_probe(dev, PTR_ERR(hhi),
				     "failed to get HHI regmap\n");

	phy = meson_hdmi_phy_alloc(dev, hhi, data);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy),
				     "failed to create PHY\n");

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id meson_hdmi_phy_of_match[] = {
	{ .compatible = "amlogic,meson-gxbb-hdmi-tx-phy", .data = &gxbb_data },
	{ .compatible = "amlogic,meson-gxl-hdmi-tx-phy",  .data = &gxl_data },
	{ .compatible = "amlogic,meson-g12a-hdmi-tx-phy", .data = &g12a_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_hdmi_phy_of_match);

static struct platform_driver meson_hdmi_phy_driver = {
	.probe = meson_hdmi_phy_probe,
	.driver = {
		.name = "meson-hdmi-tx-phy",
		.of_match_table = meson_hdmi_phy_of_match,
	},
};
module_platform_driver(meson_hdmi_phy_driver);

MODULE_AUTHOR("Ronald Claveau <linux-kernel-dev@aliel.fr>");
MODULE_DESCRIPTION("Amlogic Meson HDMI TX PHY driver");
MODULE_LICENSE("GPL");
