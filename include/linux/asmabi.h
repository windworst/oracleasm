/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmabi.h - Oracle Automatic Storage Management library kernel<->user ABI
    
  DESCRIPTION
  
  This file is an internal header to the asmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _ASMABI_H
#define _ASMABI_H


/*
 * Structures
 */

/*
 * These are __u64 to handle 32<->64 pointer stuff.
 */
struct oracleasm_io
{
    __u64               io_handle;	/* osm_ctx */
    __u64               io_requests;	/* osm_ioc ** */
    __u64               io_waitreqs;	/* osm_ioc ** */
    __u64               io_completions;	/* osm_ioc ** */
    __u64               io_timeout;	/* struct timespec * */
    __u64               io_statusp;	/* __u32 * */
    __u32               io_reqlen;
    __u32               io_waitlen;
    __u32               io_complen;
    __u32               io_pad1;	/* Pad to 64bit aligned size */
};

struct oracleasm_disk_query
{
    __u64 dq_rdev;
    __u64 dq_maxio;  /* gcc padding is lame */
};

#define ASM_ABI_VERSION	1UL
struct oracleasm_get_iid
{
    __u64 gi_iid;
    __u64 gi_version;  /* gcc padding is lame */
};



/*
 * ioctls
 */
#define ASM_IOCTL_BASE          0xFD

/* ioctls on /dev/oracleasm */
#define ASMIOC_GETIID           _IOR(ASM_IOCTL_BASE, 0, struct oracleasm_get_iid)
#define ASMIOC_CHECKIID         _IOWR(ASM_IOCTL_BASE, 1, struct oracleasm_get_iid)

/* ioctls on /dev/oracleasm/<iid> */
#define ASMIOC_QUERYDISK        _IOWR(ASM_IOCTL_BASE, 2, struct oracleasm_disk_query)
#define ASMIOC_OPENDISK		_IOWR(ASM_IOCTL_BASE, 3, struct oracleasm_disk_query)
#define ASMIOC_CLOSEDISK	_IOW(ASM_IOCTL_BASE, 4, struct oracleasm_disk_query)


/*
 * We have separate ioctls so we *know* when the pointers are 32bit
 * or 64bit.
 * 
 * All userspace callers should use ASMIOC_IODISK.
 */
#define ASMIOC_IODISK32         _IOWR(ASM_IOCTL_BASE, 5, struct oracleasm_io)

#if BITS_PER_LONG == 32
# define ASMIOC_IODISK ASMIOC_IODISK32
#else
# if BITS_PER_LONG == 64
#  define ASMIOC_IODISK64         _IOWR(ASM_IOCTL_BASE, 6, struct oracleasm_io)
#  define ASMIOC_IODISK ASMIOC_IODISK64
# else
#  error Invalid number of bits (BITS_PER_LONG)
# endif  /* BITS_PER_LONG == 64 */
#endif  /* BITS_PER_LONG == 32 */


/* ioctl for testing */
#define ASMIOC_DUMP             _IO(ASM_IOCTL_BASE, 16)


#endif  /* _ASMABI_H */

