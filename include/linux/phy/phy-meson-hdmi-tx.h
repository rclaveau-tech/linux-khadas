/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __PHY_MESON_HDMI_TX_H
#define __PHY_MESON_HDMI_TX_H

struct device;
struct regmap;

enum meson_hdmi_phy_soc {
	MESON_HDMI_PHY_GXBB,
	MESON_HDMI_PHY_GXL,	/* GXL and GXM */
	MESON_HDMI_PHY_G12A,
};

/* DEPRECATED: compat for DTs without "phys" on the HDMI TX node */
struct phy *meson_hdmi_phy_create_legacy(struct device *dev,
					 struct regmap *hhi,
					 enum meson_hdmi_phy_soc soc);

#endif /* __PHY_MESON_HDMI_TX_H */
