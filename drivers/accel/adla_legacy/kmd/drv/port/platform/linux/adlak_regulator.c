// SPDX-License-Identifier: GPL-2.0-only
/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/*
 *
 * @file adlak_regulator.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a sh nn team 2024/04/09	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
/* adjust nn voltage */
#include <linux/arm-smccc.h>
#include <linux/regulator/consumer.h>

#include "adlak_regulator.h"
#include "adlak_os_base.h"
#include "adlak_device.h"
/**************************** Type Definitions *******************************/
enum nn_regulator_type_t {
	regulator_none = 0,
	regulator_gpio,
	regulator_pwm
};

enum nn_hw_version_t {
	adla_hw_ver_default = 0,
	adla_hw_ver_r0p0,
	adla_hw_ver_r1p0,
	adla_hw_ver_r2p0,
};

enum nn_efuse_type_t {
	adla_efuse_type_disable = 0,
	adla_efuse_type_ss = 1,
	adla_efuse_type_tt = 2,
	adla_efuse_type_ff = 3,
};

/***************** Macros (Inline Functions) Definitions *********************/
#define GPIO_REGULATOR_NAME "vdd_npu"
#define PWM_REGULATOR_NAME "VDD_NPU"
/************************** Variable Definitions *****************************/
struct regulator *nn_regulator;
int nn_regulator_flag;

/************************** Function Prototypes ******************************/

static unsigned int adlak_get_nn_efuse_chip_type(u64 function_id, u64 arg0,
						 u64 arg1, u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id, (unsigned long)arg0,
		      (unsigned long)arg1, (unsigned long)arg2, 0, 0, 0, 0,
		      &res);
	return res.a0;
}

static enum nn_regulator_type_t adlak_regulator_nn_available(struct device *dev)
{
#ifdef CONFIG_OF
	const char *regulator_name = NULL;

	if (of_property_read_string(dev->of_node, "nn_regulator", &regulator_name))
		return regulator_none;

	if (!strcmp(regulator_name, "gpio_regulator"))
		return regulator_gpio;

	if (!strcmp(regulator_name, "pwm_regulator"))
		return regulator_pwm;
#endif
	return regulator_none;
}

static int adlak_voltage_adjust_r1p0(struct adlak_device *padlak)
{
	int ret = -1;
	enum nn_regulator_type_t regulator_type;

	regulator_type = adlak_regulator_nn_available(padlak->dev);

	switch (regulator_type) {
	case regulator_gpio:
		nn_regulator =
			devm_regulator_get(padlak->dev, GPIO_REGULATOR_NAME);
		pr_info("ADLA KMD nna regulator by gpio.\n");
		break;
	case regulator_pwm:
		nn_regulator =
			devm_regulator_get(padlak->dev, PWM_REGULATOR_NAME);
		pr_info("ADLA KMD nna regulator by pwm.\n");
		break;
	case regulator_none:
		nn_regulator = NULL;
		ret = 0;
		AML_LOG_INFO("ADLA KMD voltage regulator disable\n");
		pr_info("ADLA KMD nna regulator disable.\n");
		break;
	}

	if (!ret)
		return ret;

	if (IS_ERR(nn_regulator)) {
		ret = -1;
		nn_regulator = NULL;
		pr_err("regulator_get vddnpu fail!\n");
		return ret;
	}

	ret = regulator_enable(nn_regulator);
	if (ret < 0) {
		pr_err("regulator_enable error\n");
		devm_regulator_put(nn_regulator);
		nn_regulator = NULL;
		return ret;
	}

	return ret;
}

static int adlak_voltage_adjust_r2p0(struct adlak_device *padlak)
{
/*****************************************************************************
 * pwm              0%    %5    %10   15%   20%   25%   30%   35%   40%   45%
 * board v1 vol(v)  0.89  0.88  0.87  0.86  0.85  0.84  0.83  0.82  0.81  0.80
 * board v2 vol(v)  0.93  0.92  0.91  0.90  0.89  0.88  0.87  0.86  0.85  0.84
 * reg value
 *****************************************************************************/
/*****************************************************************************
 * pwm              50%   55%   60%   65%   70%   75%   80%   85%   90%   95%
 * board v1 vol(v)  0.79  0.78  0.77  0.76  0.75  0.74  0.73  0.72  0.71  0.70
 * board v2 vol(v)  0.83  0.82  0.81  0.80  0.79  0.78  0.77  0.76  0.75  0.74
 * reg value
 *****************************************************************************/
#define NN_T7C_BOARD_V1_ID 1
#define NN_T7C_BOARD_V2_ID 2
#define NN_T7C_BOARD_V1_890MV 890000
#define NN_T7C_BOARD_V1_870MV 870000
#define NN_T7C_BOARD_V2_910MV 870000
#define NN_T7C_BOARD_V2_890MV 850000
#define NN_T7C_BOARD_V2_870MV 830000
#define NN_T7C_BOARD_V2_850MV 810000
#define NN_EFUSE_TYPE_NPU 2
#define NN_GET_DVFS_TABLE_INDEX 0x82000088
#define NN_MESON_CPU_VERSION_LVL_PACK 2
#define NN_T7C_PACKAGE_TYPE_A311D2 1
#define NN_T7C_PACKAGE_TYPE_POP1 2
#define NN_T7C_PACKAGE_TYPE_V918D 3
#define NN_T7C_PACKAGE_TYPE_A311D2J 4
	int ret = 0;
	int nn_voltage_value = 0;
	unsigned int nn_package_id;
	unsigned int nn_efuse_type = 0;

	//nn_package_id = get_meson_cpu_version(NN_MESON_CPU_VERSION_LVL_PACK);
	nn_package_id = 1;
	nn_efuse_type = adlak_get_nn_efuse_chip_type(NN_GET_DVFS_TABLE_INDEX,
						     NN_EFUSE_TYPE_NPU, 0, 0);
	pr_info("ADLA KMD nn_adj_vol = %d, nn_package_id = %u, nn_efuse_type = %u\n",
		nn_regulator_flag, nn_package_id, nn_efuse_type);

	nn_regulator = devm_regulator_get(padlak->dev, PWM_REGULATOR_NAME);
	if (IS_ERR(nn_regulator)) {
		ret = -1;
		nn_regulator = NULL;
		pr_err("regulator_get vddnpu fail!\n");
		return ret;
	}

	ret = regulator_enable(nn_regulator);
	if (ret < 0) {
		pr_err("regulator_enable error\n");
		devm_regulator_put(nn_regulator);
		nn_regulator = NULL;
		return ret;
	}

	/* nn_regulator_flag == 0 board version is v1(old legacy board) */
	if (!nn_regulator_flag) {
		if (nn_package_id == NN_T7C_PACKAGE_TYPE_A311D2J)
			nn_voltage_value = NN_T7C_BOARD_V1_870MV;
		else
			nn_voltage_value = NN_T7C_BOARD_V1_890MV;
	} else {
		/* nn_regulator_flag == 1 board version is v2
		 * or other customer ver (new type board)
		 */
		if (nn_package_id == NN_T7C_PACKAGE_TYPE_A311D2J) {
			nn_voltage_value = NN_T7C_BOARD_V2_870MV;
		} else {
			switch ((enum nn_efuse_type_t)nn_efuse_type) {
			case adla_efuse_type_ss:
				nn_voltage_value = NN_T7C_BOARD_V2_910MV;
				break;
			case adla_efuse_type_tt:
				nn_voltage_value = NN_T7C_BOARD_V2_870MV;
				break;
			case adla_efuse_type_ff:
				nn_voltage_value = NN_T7C_BOARD_V2_850MV;
				break;
			default:
				/* if no efuse id, PDVFS is disable,
				 * we set default voltage to 890mv
				 */
				nn_voltage_value = NN_T7C_BOARD_V2_890MV;
				break;
			}
		}
	}
	if (nn_voltage_value) {
		ret = regulator_set_voltage(nn_regulator, nn_voltage_value,
					    nn_voltage_value);
	}
	if (ret < 0) {
		regulator_disable(nn_regulator);
		devm_regulator_put(nn_regulator);
		nn_regulator = NULL;
		pr_err("regulator_set_voltage %dmv Error\n", nn_voltage_value);
	} else {
		pr_info("regulator_set_voltage %dmv OK\n", nn_voltage_value);
	}

	return ret;
}

static int adlak_voltage_adjust_default(struct adlak_device *padlak)
{
	nn_regulator = NULL;
	return 0;
}

static enum nn_hw_version_t adlak_get_nn_hw_version(struct device *dev)
{
#ifdef CONFIG_OF
	const char *adla_hw_ver_name = NULL;

	if (of_property_read_string(dev->of_node, "nn_hw_version", &adla_hw_ver_name))
		return adla_hw_ver_default;

	if (!strcmp(adla_hw_ver_name, "r2p0"))
		return adla_hw_ver_r2p0;

	if (!strcmp(adla_hw_ver_name, "r1p0"))
		return adla_hw_ver_r1p0;
#endif
	return adla_hw_ver_default;
}

int adlak_voltage_init(void *data)
{
	int ret = 0;
	struct adlak_device *padlak = (struct adlak_device *)data;
	enum nn_hw_version_t hw_ver;

	hw_ver = adlak_get_nn_hw_version(padlak->dev);

	switch (hw_ver) {
	case adla_hw_ver_r2p0:
		ret = adlak_voltage_adjust_r2p0(padlak);
		break;
	case adla_hw_ver_r1p0:
		ret = adlak_voltage_adjust_r1p0(padlak);
		break;
	case adla_hw_ver_r0p0:
	case adla_hw_ver_default:
		ret = adlak_voltage_adjust_default(padlak);
		break;
	}

	if (!ret)
		pr_info("ADLA KMD voltage init success ");

	return ret;
}

int adlak_voltage_uninit(void *data)
{
	int ret = 0;

	if (nn_regulator) {
		ret = regulator_disable(nn_regulator);
		if (ret < 0)
			pr_err("regulator_disable error\n");

		devm_regulator_put(nn_regulator);
	}

	if (!ret)
		pr_info("ADLA KMD voltage uninit success ");

	return ret;
}
