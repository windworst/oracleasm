/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmabi.h - Oracle Storage Manager kernel<->user ABI
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _OSMABI_H
#define _OSMABI_H


/*
 * Structures
 */

/*
 * These are __u64 to handle 32<->64 pointer stuff.
 */
struct osmio
{
    __u64               oi_handle;	/* osm_ctx */
    __u64               oi_requests;	/* osm_ioc ** */
    __u64               oi_waitreqs;	/* osm_ioc ** */
    __u64               oi_completions;	/* osm_ioc ** */
    __u64               oi_timeout;	/* struct timespec * */
    __u64               oi_statusp;	/* __u32 * */
    __u32               oi_reqlen;
    __u32               oi_waitlen;
    __u32               oi_complen;
    __u32               oi_pad1;	/* Pad to 64bit aligned size */
};

struct osm_disk_query
{
    __u64 dq_rdev;
    __u64 dq_maxio;  /* gcc padding is lame */
};

#define OSM_ABI_VERSION	1UL
struct osm_get_iid
{
    __u64 gi_iid;
    __u64 gi_version;  /* gcc padding is lame */
};



/*
 * ioctls
 */
#define OSM_IOCTL_BASE          0xFD

/* ioctls on /dev/osm */
#define OSMIOC_GETIID           _IOR(OSM_IOCTL_BASE, 0, struct osm_get_iid)
#define OSMIOC_CHECKIID         _IOWR(OSM_IOCTL_BASE, 1, struct osm_get_iid)

/* ioctls on /dev/osm/<iid> */
#define OSMIOC_QUERYDISK        _IOWR(OSM_IOCTL_BASE, 2, struct osm_disk_query)
#define OSMIOC_OPENDISK		_IOWR(OSM_IOCTL_BASE, 3, struct osm_disk_query)
#define OSMIOC_CLOSEDISK	_IOW(OSM_IOCTL_BASE, 4, struct osm_disk_query)


/*
 * We have separate ioctls so we *know* when the pointers are 32bit
 * or 64bit.
 * 
 * All userspace callers should use OSMIOC_IODISK.
 */
#define OSMIOC_IODISK32         _IOWR(OSM_IOCTL_BASE, 5, struct osmio)

#if BITS_PER_LONG == 32
# define OSMIOC_IODISK OSMIOC_IODISK32
#else
# if BITS_PER_LONG == 64
#  define OSMIOC_IODISK64         _IOWR(OSM_IOCTL_BASE, 6, struct osmio)
#  define OSMIOC_IODISK OSMIOC_IODISK64
# else
#  error Invalid number of bits (BITS_PER_LONG)
# endif  /* BITS_PER_LONG == 64 */
#endif  /* BITS_PER_LONG == 32 */


/* ioctl for testing */
#define OSMIOC_DUMP             _IO(OSM_IOCTL_BASE, 16)


#endif  /* _OSMABI_H */

