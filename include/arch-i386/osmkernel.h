/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
	NAME
	  osmkernel.h - Oracle Storage Manager structures in the kernel.
	  
	DESCRIPTION
	
	This file describes structures that are private to the Linux kernel
	module for osmlib.  See osmlib.h for field descriptions.

        THESE STRUCTURS MUST BE ABI COMPATIBLE WITH THE osmlib.h
        DEFINITION!!!

*/


#ifndef _OSMKERNEL_H
#define _OSMKERNEL_H

/*
 * Disk/Fence Keys - (unused as yet)
 */
typedef struct _osm_check osm_check;
struct _osm_check
{
	__u32		fence_num_osm_check;
	__u32		fence_value_osm_check;
	__u32		key_num_osm_check;
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
typedef struct _osm_ioc osm_ioc;
struct _osm_ioc {
	__u32		ccount_osm_ioc;
	__s32		error_osm_ioc;
	__u32		elaptime_osm_ioc;
	__u16		status_osm_ioc;
	__u16		flags_osm_ioc;
	__u8		operation_osm_ioc;
	__u8		priority_osm_ioc;
	__u16		hint_osm_ioc;
	osm_ioc	*link_osm_ioc;
	__u64   	disk_osm_ioc;
	__u64		first_osm_ioc;
	__u32		rcount_osm_ioc;
	void		*buffer_osm_ioc;
	osm_check	*check_osm_ioc;
	__u16		content_offset_osm_ioc;
	__u16		content_repeat_osm_ioc;
	__u32		content_osm_ioc;
	union
	{
		__u64	reserved_osm_ioc_8;
		struct
		{
			 __u32 reserved_osm_ioc_4_0;
			 __u32 reserved_osm_ioc_4_1;
		} reserved_osm_ioc_4;
	} reserved_osm_ioc_u;
#define reserved_osm_ioc_high reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_0
#define reserved_osm_ioc_low reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_1
};

#endif  /* _OSMKERNEL */
