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
 * osm_ioc is defined for userspace in osmlib.h and for
 * kernelspace in linux/osmstructures.h
 */
struct osmio
{
    __u64               handle;
    osm_ioc             **requests;
    __u32               reqlen;
    osm_ioc             **waitreqs;
    __u32               waitlen;
    osm_ioc             **completions;
    __u32               complen;
    __u32               intr;
    struct timespec     *timeout;
    unsigned int        *statusp;
};

struct osm_disk_query
{
    __u64 dq_rdev;
    __u32 dq_maxio;
};

struct osm_get_iid
{
    __u64 gi_iid;
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
#define OSMIOC_IODISK           _IOWR(OSM_IOCTL_BASE, 5, struct osmio)

/* ioctl for testing */
#define OSMIOC_DUMP             _IO(OSM_IOCTL_BASE, 16)


#endif  /* _OSMABI_H */

