/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is s__uject to change.
 */

/*
	NAME
	  osmkernel.h - Oracle Storage Manager structures in the kernel.
	  
	DESCRIPTION
	
	This file describes structures that are private to the Linux kernel
	module for osmlib.  See osmlib.h for field descriptions.

*/


#ifndef _OSMKERNEL_H
#define _OSMKERNEL_H

/*
 * Disk/Fence Keys - (unused as yet)
 */
typedef struct _osm_kcheck osm_kcheck;
struct _osm_kcheck
{
	__u32		fence_num_osm_kcheck;
	__u32		fence_value_osm_kcheck;
	__u32		key_num_osm_kcheck;
	__u64		key_mask_osm_check;
	__u64		key_value_osm_check;
	__u64		key_and_osm_check;
	__u64		key_or_osm_check;
	__u32		error_fence_osm_check;
	__u64		error_key_osm_check;
};


/*
 * I/O control block
 */
typedef struct _osm_kioc osm_kioc;
struct _osm_kioc {
	__u32		ccount_osm_kioc;
	__s32		error_osm_kioc;
	__u32		elaptime_osm_kioc;
	__u16		status_osm_kioc;
	__u16		flags_osm_kioc;
	__u8		operation_osm_kioc;
	__u8		priority_osm_kioc;
	__u16		hint_osm_kioc;
	osm_kioc	*link_osm_kioc;
	__u64   	disk_osm_kioc;
	__u64		first_osm_kioc;
	__u32		rcount_osm_kioc;
	void		*buffer_osm_kioc;
	osm_kcheck	*check_osm_kioc;
	__u16		content_offset_osm_kioc;
	__u16		content_repeat_osm_kioc;
	__u32		content_osm_kioc;
	union
	{
		__u64	reserved_osm_kioc_8;
		struct
		{
			 __u32 reserved_osm_kioc_4_0;
			 __u32 reserved_osm_kioc_4_1;
		} reserved_osm_kioc_4;
	} reserved_osm_kioc_u;
#define reserved_osm_kioc_high reserved_osm_kioc_u.reserved_osm_kioc_4.reserved_osm_kioc_0
#define reserved_osm_kioc_low reserved_osm_kioc_u.reserved_osm_kioc_4.reserved_osm_kioc_1
};

#endif  /* _OSMKERNEL */
