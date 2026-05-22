/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __ADLA_DRM_H__
#define __ADLA_DRM_H__

#include "drm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct drm_adla_submit {
	__u32 output_modules;
	__u32 active_modules;
	__u32 fence_modules;
	__u32 dep;
	__u32 config_size;
	__u64 config_ptr;
	__u32 fixup_count;
	__u32 pad;
	__u64 fixup_ptr;
	__u32 out_job_id;
	__u32 pad2;
};

struct drm_adla_fixup {
	__u32 blob_offset;
	__u32 pad;
	__u64 iova;
};

struct drm_adla_wait {
	__u32 job_id;
	__u32 pad;
	__s64 timeout_ns;
};

#define DRM_ADLA_SUBMIT		0x00
#define DRM_ADLA_WAIT		0x01

#define DRM_IOCTL_ADLA_SUBMIT	DRM_IOWR(DRM_COMMAND_BASE + DRM_ADLA_SUBMIT, \
					struct drm_adla_submit)
#define DRM_IOCTL_ADLA_WAIT	DRM_IOWR(DRM_COMMAND_BASE + DRM_ADLA_WAIT, \
					 struct drm_adla_wait)

#ifdef __cplusplus
}
#endif

#endif /* __ADLA_DRM_H__ */
