/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmprivate.h - Oracle Storage Manager internal library header
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _OSMPRIVATE_H
#define _OSMPRIVATE_H

/*
 * Defines
 */
#define OSMLIB_NAME "OSM Library - Generic Linux"
#define OSMLIB_MAJOR 0  /* Version should be updated to Oracle style */
#define OSMLIB_MINOR 1
#define OSMLIB_MICRO 0

#define REAL_IID(_iid)  ((unsigned long)((_iid) & 0xFFFFFFFFULL))


/*
 * ioctls
 */
#define OSM_IOCTL_BASE          0xFD

/* ioctls on /dev/osm */
#define OSMIOC_GETIID           _IOR(OSM_IOCTL_BASE, 0, long)
#define OSMIOC_CHECKIID         _IOWR(OSM_IOCTL_BASE, 1, long)

/* ioctls on /dev/osm/<iid> */
#define OSMIOC_ISDISK           _IOW(OSM_IOCTL_BASE, 2, int)


#endif  /* _OSMPRIVATE_H */
