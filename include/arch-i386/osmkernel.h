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
 * OSM Defines
 */

/* i/o status bits */
#define OSM_BUSY         0x0001                       /* too busy to process */
#define OSM_SUBMITTED    0x0002          /* request submitted for processing */
#define OSM_COMPLETED    0x0004                         /* request completed */
#define OSM_FREE         0x0008                            /* memory is free */
#define OSM_CANCELLED    0x0010                         /* request cancelled */
#define OSM_ERROR        0x0020              /* request failed with an error */
#define OSM_PARTIAL      0x0040                   /* only a partial transfer */
#define OSM_BADKEY       0x0080                         /* disk key mismatch */
#define OSM_FENCED       0x0100      /* I/O was not allowed by the fence key */

/* special timeout values */
#define    OSM_NOWAIT    0x0                   /* return as soon as possible */
#define    OSM_WAIT      0xffffffff                         /* never timeout */

/* status flags indicating reasons for return */
#define    OSM_IO_POSTED    0x1                       /* posted to run by OS */
#define    OSM_IO_TIMEOUT   0x2                                   /* timeout */
#define    OSM_IO_WAITED    0x4                        /* wait list complete */
#define    OSM_IO_FULL      0x8                      /* completion list full */
#define    OSM_IO_IDLE      0x10                       /* no more active I/O */

/* I/O operations */
#define OSM_NOOP        0x00            /* no-op to key check or pass a hint */
#define OSM_READ        0x01                          /* Read data from disk */
#define OSM_WRITE       0x02                           /* write data to disk */
#define OSM_COPY        0x03       /* copy data from one location to another */
#define OSM_GETKEY      0x04           /* get value of one or more disk keys */
#define OSM_SETKEY      0x05           /* set value of one or more disk keys */
#define OSM_GETFENCE    0x06          /* get value of one or more fence keys */
#define OSM_SETFENCE    0x07          /* set value of one or more fence keys */



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
	osm_ioc	       *link_osm_ioc;
	__u64   	disk_osm_ioc;
	__u64		first_osm_ioc;
	__u32		rcount_osm_ioc;
	unsigned long	buffer_osm_ioc;
	osm_check      *check_osm_ioc;
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
#if defined(__LITTLE_ENDIAN)
#define reserved_osm_ioc_high reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_1
#define reserved_osm_ioc_low reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_0
#else
#if defined(__BIG_ENDIAN)
#define reserved_osm_ioc_high reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_0
#define reserved_osm_ioc_low reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_1
#else
#error BYTESEX
#endif
#endif

#define request_key_osm_ioc reserved_osm_ioc_low
};

#endif  /* _OSMKERNEL */


