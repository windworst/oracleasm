/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmstructures.h - Oracle Storage Manager internal library structures
    
  DESCRIPTION
  
  This file describes structures that are private to the Linux
  implementation of osmlib.  Some structures are identical in size and
  arrangement to the structures in osmlib.h but are provided here with
  elements that better represent the architecture (eg, mapping a ub8 to
  two ub4s on x86 because a pointer on x86 is only that big).  Those
  structures will be noted as such.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _OSMSTRUCTURES_H
#define _OSMSTRUCTURES_H

/*
 * Private context pointer
 *
 * This is the internal representation of a context.  It will be casted
 * from the osm_ctx exposed to the user of osmlib.
 */
typedef struct _osm_ctx_private osm_ctx_private;
struct _osm_ctx_private
{
    osm_iid iid;
    int fd;
    int discover_index;
    void *discover_cache;
    char posted;
};


/*
 * Disk name and attributes structure
 *
 * This structure is a mirror of the osm_name structure in osmlib.h.
 * It must be binary identical to that structure.
 */
typedef struct _osm_name_private osm_name_private;

struct _osm_name_private
{
  uword   interface_osm_name;                /* osmlib interfaces supported */
  oratext label_osm_name[OSM_MAXLABEL];                        /* Disk label */
  oratext udid_osm_name[OSM_MAXUDID];                    /* Unique Disk ID */
  oratext path_osm_name[OSM_MAXPATH];                    /* Access path name */
  oratext fgroup_osm_name[OSM_MAXFGROUP];              /* Failure group name */
  ub4     blksz_osm_name;                 /* physical block size of the disk */
  ub4     maxio_osm_name;                 /* Maximum atomic I/O to the disk in blocks */
  ub4     max_abs_osm_name;               /* Maximum applicaiton block size in blocks */
  ub8     size_osm_name;                              /* disk size in blocks */
  ub4     keys_osm_name;                    /* number of disk keys supported */
  ub4     fences_osm_name;                 /* number of fence keys supported */
  ub2     tags_osm_name;                 /* number of partition tags supported */
  ub2     mirror_osm_name;            /* mirrored disks underlying this disk */
  ub2     parity_osm_name;        /* Data blocks covered by one parity block */
  ub2     speed_osm_name;       /* Max transfer rate in megabytes per second */
  union
  {
      ub8 reserved_osm_name_8;
      struct
      {
          ub4 reserved_osm_name_4_0, reserved_osm_name_4_1;
      } reserved_osm_name_4;
  } reserved_osm_name_u;
#define reserved_osm_name_high reserved_osm_name_u.reserved_osm_name_4.reserved_osm_name_4_1
#define reserved_osm_name_low reserved_osm_name_u.reserved_osm_name_4.reserved_osm_name_4_0
};


/*
 * I/O control block
 */
typedef struct _osm_ioc_private   osm_ioc_private;
struct _osm_ioc_private {
  ub4       ccount_osm_ioc;                               /* Completed count */
  osm_erc    error_osm_ioc;                                    /* Error code */
  osm_erc    warn_osm_ioc;                                    /* Warning error code */
  ub4    elaptime_osm_ioc;                                   /* Elapsed Time */
  ub2     status_osm_ioc;                           /* status of the request */
  ub2     flags_osm_ioc;   /* flags set by Oracle describing the i/o request */
  ub1    operation_osm_ioc;                           /* which I/O operation */
  ub1   priority_osm_ioc;                                        /* Priority */
  ub2     hint_osm_ioc;                                      /* caching hint */
  osm_ioc *link_osm_ioc;          /* pointer to destination osm_ioc for copy */
  osm_handle disk_osm_ioc;                                 /* disk to access */
  ub8     first_osm_ioc;           /* first block, key, or fence of transfer */
  ub4     rcount_osm_ioc;                                   /* Request count */
  void   *buffer_osm_ioc;                 /* buffer address for the transfer */
  osm_check *check_osm_ioc;                 /* pointer to check data/results */
  ub2     xor_osm_ioc;                 /* 16 bit XOR of entire data transfer */
  ub2     abs_osm_ioc;          /* application block size in physical blocks */

  ub4     abn_offset_osm_ioc;     /* byte offset to application block number */
  ub4     abn_osm_ioc;            /* value of first application block number */
  ub4     abn_mask_osm_ioc;                      /* mask to limit comparison */
  ub8     tag_osm_ioc;                /* expected tag if this is a disk write */
  union
  {
      ub8 reserved_osm_ioc_8;
      struct
      {
          ub4 reserved_osm_ioc_4_0, reserved_osm_ioc_4_1;
      } reserved_osm_ioc_4;
  } reserved_osm_ioc_u;
#define reserved_osm_ioc_high reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_1
#define reserved_osm_ioc_low reserved_osm_ioc_u.reserved_osm_ioc_4.reserved_osm_ioc_4_0
};

#endif  /* _OSMSTRUCTURES */
