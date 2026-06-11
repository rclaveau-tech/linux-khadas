/* SPDX-License-Identifier: GPL-2.0-only */
/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_api_base.h
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

#ifndef __ADLAK_API_BASE_H__
#define __ADLAK_API_BASE_H__

/***************************** Include Files *********************************/

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

enum adlak_smmu_tlb_type {
	ADLAK_ENUM_SMMU_TLB_TYPE_PUBLIC_ONLY = 0, // default setting
	ADLAK_ENUM_SMMU_TLB_TYPE_PRIVATE_ONLY,
	ADLAK_ENUM_SMMU_TLB_TYPE_PRIVATE_AND_PUBLIC,
} __packed;

struct adlak_buf_desc {
	u64 iova_addr; /* virtual address in smmu*/
	u64 va_user; /* virtual address in user mode*/
	u64 phys_addr; /* physical base address if mem_type is contiguous*/
	u64 bytes; /*return real size*/
	u64 uid;
} __packed;

enum adlak_mem_type {
	ADLAK_ENUM_MEMTYPE_CACHEABLE = (1 << 0),
	ADLAK_ENUM_MEMTYPE_CONTIGUOUS = (1 << 1),
	ADLAK_ENUM_MEMTYPE_INNER =
		(1 << 2), // For ADLA use only if value is true.
	ADLAK_ENUM_MEMTYPE_PA_WITHIN_4G =
		(1 << 4), // physical address less than 4Gbytes
	ADLAK_ENUM_MEMTYPE_SHARE = (1 << 5), // share between different models
	ADLAK_ENUM_MEMTYPE_SMMU_TLB_DEF = (1 << 6), //
	ADLAK_ENUM_MEMTYPE_SMMU_TLB_ID1 = (1 << 7), //
	ADLAK_ENUM_MEMTYPE_SMMU_PRIV = (1 << 8) //
} __packed;

enum adlak_mem_direction {
	ADLAK_ENUM_MEM_DIR_READ_WRITE = 0,
	ADLAK_ENUM_MEM_DIR_READ_ONLY,
	ADLAK_ENUM_MEM_DIR_WRITE_ONLY
} __packed;

struct adlak_buf_req {
	u64 mem_handle; /* return memory info handle in kernel */
	u64 bytes; /* bytes requested to allocate */
	u32 align_in_page; /* alignment requirements (in 4KB) */
	u32 data_type; /* type of data in the buffer to allocate */
	u32 mem_type; /*request info*/
	u32 mem_direction; /*request info*/
	struct adlak_buf_desc
		ret_desc; /* info of buffer successfully allocated */
	u32 mmap_en; /* the flag of mmap */
	u32 errcode; /* return err number */
} __packed;

struct adlak_extern_buf_info {
	u64 buf_handle; /* buf handle */
	u64 mem_handle; /* return memory info handle in kernel */
	u64 bytes; /* bytes of buffer */
	u32 buf_type; /* type of buf handle */
	u32 mem_type; /*request info*/
	u32 mem_direction; /*request info*/
	struct adlak_buf_desc ret_desc; /* info of buffer successfully import */
	u32 mmap_en; /* the flag of mmap */
	u32 errcode; /* return err number */
} __packed;

enum adlak_flush_cache_direction {
	FLUSH_TO_DEVICE = 1,
	FLUSH_FROM_DEVICE = 2,
	FLUSH_NONE = 3,
};

struct adlak_buf_flush {
	u64 mem_handle; /* info of buffer  */
	u32 direction;
	u32 is_partial; /* is dma sync partial*/
	u64 offset;
	u64 size;
	u32 errcode; /* return err number */
} __packed;

struct adlak_cmd_buf_attr {
	s32 support;
	u32 reserve_count_modify_head;
	u32 reserve_count_modify_tail;
	u32 reserve_count_common_head;
	u32 reserve_count_common_tail;
	u64 mem_handle;
} __packed;

enum adlak_context_priority {
	ADLAK_CONTEXT_PRIORITY_DEFAULT = 0,
	ADLAK_CONTEXT_PRIORITY_HIGH,
};

enum adlak_cmq_buffer_type {
	ADLAK_CMQ_BUFFER_TYPE_PRIVATE = 0,
	ADLAK_CMQ_BUFFER_TYPE_PUBLIC,
};

struct adlak_network_desc {
	s32 config_total_size;
	s32 dep_fixups_num;
	s32 reg_fixups_num;
	s32 tasks_num;
	u64 config_va;
	u64 dep_fixups_va;
	u64 reg_fixups_va;
	u64 tasks_va;
	s32 profile_en; // profilling enable
	u64 profile_iova;
	u32 profile_buf_size;
	s32 cmq_buffer_type;
	u32 priority; // submit priority
	s32 net_register_idx; // return from kmd
	s32 hw_last_layer_in_first_smmu;
	s64 macc_count;
	struct adlak_cmd_buf_attr cmd_buf_attr;

} __packed;

struct adlak_network_del_desc {
	s32 net_register_idx;
} __packed;

struct adlak_network_invoke_desc {
	s32 net_register_idx;
	s32 invoke_register_idx; // return from kmd
	s32 start_idx;
	s32 end_idx;
	s32 addr_fixups_num;
	u64 addr_fixups_va;

} __packed;

struct adlak_network_invoke_del_desc {
	s32 net_register_idx;
	s32 invoke_register_idx;

} __packed;

struct adlak_get_stat_desc {
	s32 net_register_idx;
	s32 invoke_register_idx;
	s32 timeout_ms;
	s32 start_idx; // return from kmd
	s32 end_idx; // return from kmd
	s32 ret_state; // 0: success,1:running,-1: timeout, -3: other err
	s32 profile_en; // profilling enable
	u32 profile_rpt; // deprecated
	s32 invoke_time_us; // invoke time which get from os

	u64 axi_freq_cur; // adlak axi clock frequency currently
	u64 core_freq_cur; // adlak core clock frequency currently
	u64 mem_alloced_base; // alloced by kmd
	u64 mem_alloced_umd; // alloced by umd in this context
	s64 mem_pool_size; //-1:the limit base on the system
	u64 mem_pool_used; // memory usage
	s32 efficiency;
} __packed;

struct adlak_profile_cfg_desc {
	s32 net_register_idx;
	s32 profile_en; // profilling enable
	u64 profile_iova;
	u32 profile_buf_size;
	u32 errcode; /* return err number */

} __packed;

struct adlak_test_desc {
	u64 type;
} __packed;

struct adlak_caps_desc {
	u32 hw_ver; /* adlak hardware version*/
	u64 axi_freq_max; /* adlak axi clock frequency maximum */
	u64 core_freq_max; /* adlak core clock frequency maximum */
	u32 cmq_size; /* cmq buffer size*/
	u64 sram_base; /* axi sram base addr*/
	u32 sram_size; /* axi sram buffer size*/
	u64 hw_iova_max_size; /* tha maximum vaddr value allowed by the hardware*/
	u64 iova_max_size; /* tha maximum vaddr of smmu*/
	u64 iova_free_size; /* tha free size of vaddr*/
} __packed;

struct adlak_context_attribute {
	u32 smmu_tlb_type : 8;
	u32 rsv : 24;
} __packed;

/************************** Function Prototypes ******************************/

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_API_BASE_H__ end define*/
