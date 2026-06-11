/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_io.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/04/26	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_IO_H__
#define __ADLAK_IO_H__

/***************************** Include Files *********************************/

#include <linux/io.h>
#include <asm/types.h>
#include <linux/device.h>

#include "adlak_hw.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

static inline u32 adlak_read32(struct io_region *region, ulong offset)
{
#if !(CONFIG_ADLAK_EMU_EN)
#if ADLAK_HW_DEBUG_EN
	u32 val;
#endif
	ASSERT(region);
	ASSERT(region->va_kernel);
	ASSERT(offset < region->size);
#if ADLAK_HW_DEBUG_EN
	val = readl((void __iomem *)((ulong)(region->va_kernel) + offset));
	pr_err("[ADLAK DEBUG] read  reg[0x%lX] = [0x%X].", offset, val);
	return val;
#else
	return readl((void __iomem *)((ulong)(region->va_kernel) + offset));
#endif
#else
	return 0;
#endif
}

static inline void adlak_write32(struct io_region *region, ulong offset,
				 unsigned int data)
{
	ASSERT(region);
	ASSERT(region->va_kernel);
	ASSERT(offset < region->size);
#if !(CONFIG_ADLAK_EMU_EN)
#if ADLAK_HW_DEBUG_EN
	pr_err("[ADLAK DEBUG] write reg[0x%lX] = [0x%X].", offset, data);
	writel((u32)data,
	       (void __iomem *)((ulong)(region->va_kernel) + offset));
#else
	writel((u32)data,
	       (void __iomem *)((ulong)(region->va_kernel) + offset));
#endif
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_IO_H__ end define*/
