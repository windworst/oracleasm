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
 * Max I/O is 64K.  Why 64?  Because the MegaRAID card supports only
 * 26 scatter/gather segments.  26 * 4k page == 104k.  The power of 2
 * below that is 64K.  The intention is for per-device discovery of this
 * value in the future.  FIXME.
 */
#define OSM_MAX_IOSIZE          (1024 * 64)

#define HIGH_UB4(_ub8)          ((unsigned long)(((_ub8) >> 32) & 0xFFFFFFFFULL))
#define LOW_UB4(_ub8)           ((unsigned long)((_ub8) & 0xFFFFFFFFULL))
#define REAL_IID(_iid)          (LOW_UB4((_iid)))
#define REAL_HANDLE(_hand)      (LOW_UB4((_hand)))


/*
 * Structures
 */

/*
 * osm_ioc is defined for userspace in osmlib.h and for
 * kernelspace in linux/osmstructures.h
 */
struct osmio
{
    unsigned long        handle;
    osm_ioc             **requests;
    unsigned int        reqlen;
    osm_ioc             **waitreqs;
    unsigned int        waitlen;
    osm_ioc             **completions;
    unsigned int        complen;
    __u32               intr;
    struct timespec     *timeout;
    unsigned int        *statusp;
};

struct osm_disk_query
{
    int dq_rdev;
    unsigned long dq_maxio;
};



/*
 * ioctls
 */
#define OSM_IOCTL_BASE          0xFD

/* ioctls on /dev/osm */
#define OSMIOC_GETIID           _IOR(OSM_IOCTL_BASE, 0, unsigned long)
#define OSMIOC_CHECKIID         _IOWR(OSM_IOCTL_BASE, 1, unsigned long)

/* ioctls on /dev/osm/<iid> */
#define OSMIOC_QUERYDISK        _IOWR(OSM_IOCTL_BASE, 2, struct osm_disk_query)
#define OSMIOC_OPENDISK		_IOWR(OSM_IOCTL_BASE, 3, unsigned long)
#define OSMIOC_CLOSEDISK	_IOW(OSM_IOCTL_BASE, 4, unsigned long)
#define OSMIOC_IODISK           _IOWR(OSM_IOCTL_BASE, 5, struct osmio)

/* ioctl for testing */
#define OSMIOC_DUMP             _IO(OSM_IOCTL_BASE, 16)


#endif  /* _OSMABI_H */

