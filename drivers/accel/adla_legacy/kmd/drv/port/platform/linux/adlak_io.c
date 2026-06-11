// SPDX-License-Identifier: GPL-2.0-only
/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/*
 *
 * @file adlak_io.c
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

/***************************** Include Files *********************************/

#include <linux/io.h>

#include "adlak_common.h"
#include "adlak_hw.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

struct io_region *adlak_create_ioregion(uintptr_t phys_base, u32 size)
{
	struct io_region *region = NULL;

	if (!size) {
		AML_LOG_DEBUG("invalid input args size!");
		goto fail;
	}

	region = adlak_os_zalloc(sizeof(struct io_region), ADLAK_GFP_KERNEL);
	if (!region)
		goto fail;

	if (!request_mem_region(phys_base, size, DEVICE_NAME)) {
		AML_LOG_ERR("request IO region failed");
		goto fail;
	}

	region->va_kernel = ioremap(phys_base, size);
	if (!region->va_kernel) {
		AML_LOG_ERR("ioremap failed");
		goto fail;
	}

	region->pa_kernel = phys_base;
	region->size = size;
	AML_LOG_INFO("ioremap successd PA=0x%lX, VA=0x%lX ,size=0x%x",
		     (uintptr_t)region->pa_kernel, (uintptr_t)region->va_kernel,
		     size);

	/* success */
	goto end;

fail:
	AML_LOG_ERR("creating IO region [0x%lX, 0x%lX] failed",
		    (uintptr_t)phys_base, (uintptr_t)phys_base + size - 1);

end:
	return region;
}

void adlak_destroy_ioregion(struct io_region *region)
{
	if (region && region->va_kernel) {
		iounmap(region->va_kernel);
		release_mem_region(region->pa_kernel, region->size);
		region->va_kernel = NULL;
		region->pa_kernel = 0;
		region->size = 0;
		adlak_os_free(region);
	}
}
