/* Copyright (c) 2001, 2003, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmlib.h - Oracle Storage Manager interface definition
    
  DESCRIPTION
  
  The Oracle Storage Manager (OSM) is a file system inside the Oracle kernel
  that allows Oracle to use raw disks rather than external file systems/volume
  managers for storing all the data for an Oracle database. OSM is given one
  or more disks to form a disk group. OSM will automatically allocate data
  files, control files, online logs, archive logs, backup sets, and other
  binary files from a disk group as needed. The I/O load is evenly distributed
  across all the disks in a group.  Administrators administer disk groups not
  files. OSM supports a database having its files in multiple disk groups and
  multiple databases having files in the same disk group.

  OSM will work with JBOD or storage arrays. With JBOD OSM will implement
  redundancy algorithms to tolerate disk failures.  With a storage array the
  redundancy may be implemented by the array. With JBOD each disk given to OSM
  is a single physical spindle. With a storage array each disk given to OSM
  may be aggregated from multiple disks. OSM supports add/delete of disks from
  a disk group as well as resizing a disk in a disk group. Resizing is useful
  for growing or shrinking a virtual disk given to OSM by a storage array.

  This header file defines an alternative API for the Oracle kernel to access
  raw disks, rather than the standard operating system interface. A storage
  vendor may implement a library that presents this API to enhance the
  performance and/or managability of OSM for their storage. The API provides
  six major enhancements over standard interfaces: discovery, I/O processing,
  serverless copy, metadata validation, usage hints, and write validation.

  Discovery

  osmlib allows OSM to find the disks that are already in disk groups, and
  disks that are available for adding to disk groups. The disks may be
  accessed via different names on different nodes in a cluster.  Discovery
  makes the characteristics of the disk available to OSM. Disks discovered
  through osmlib do not need to be available through normal operating system
  interfaces.

  I/O processing 

  One call to osmlib can submit, and reap multiple I/O's. This can 
  dramatically reduce the number of calls to the operating system for
  doing I/O. One handle can be used by all the processes in an
  instance for accessing the same disk. This eliminates multiple open
  calls and multiple file descriptors.
  
  Serverless copy
 
  A read and a write can be linked so that the data can be copied
  without sending it to a server. This is useful for replicating data,
  taking backups, and relocating data for load balancing. 

  Metadata validation on I/O
  
  osmlib allows disk key and fence key values to be loaded into a disk. Every
  I/O can have a disk key value and a fence key value that must match for the
  I/O to occur. Disk keys are used to down load OSM metadata into the disk for
  validation on every I/O. Changing the disk key value invalidates cached
  versions of the metadata. Fence keys are used for fencing off one dead
  Oracle instance without fencing other instances on the same node. Disk and
  fence keys are particularly useful in a cluster. Disk and fence keys do not
  have to be persistent, but must be very long lived since reloading them is
  expensive.

  Usage hints

  Each I/O request includes a hint value to describe why Oracle is doing this
  I/O. For example, one hint value indicates a write is to the online redo
  log for database update. Another hint indicates a write is to initialize
  a new online log so it can be used for logging data. The storage device can
  use these hints to choose caching policies. Each I/O also includes a
  priority so that the storage device can complete the more important requests
  first. A priority is included with every I/O to ensure the more important
  I/O's are done first.

  Write validation

  Bugs can occur in the software and hardware between the Oracle kernel and
  the storage system. The most damaging problems affect the data on a disk
  write. Bugs in operating systems, host adaptors, switches and other places
  can result in the wrong data being associated with a write. I/O requests can
  contain information about the data so that the storage can verify that it
  received the data intended to go with the command. Administrators can
  accidentally overwrite a disk that is in use.  Associating partition tags
  with disk partitions can assign a portion of a disk to a particular
  application and have the disk verify writes are from the correct
  application. Tagging will disallow writes through normal I/O mechanisms to
  tagged areas of a disk.
  
  RELATED DOCUMENTS
  
  INSPECTION STATUS 
    Inspection date:
    Inspection status:
    Estimated increasing cost defects per page:
    Rule sets:        

  ACCEPTANCE REVIEW STATUS 
    Review date:   
    Review status:
    Reviewers: 

  PUBLIC FUNCTIONS
    osm_version    - Version handshake and instance initialization
    osm_init       - Initialize osmlib for this thread of execution
    osm_fini       - Finished calling osmlib for this thread of execution
    osm_error      - Translate an error code into an error message
    osm_discover   - Discover the names of disks in a disk set
    osm_fetch      - Fetch names from the discovered set of disks
    osm_open       - Open a disk for read/write
    osm_close      - Close a disk
    osm_io         - Manage disk I/O requests
    osm_ioerror    - Translate an I/O error code into an error message
    osm_iowarn     - Translate an I/O warning code into a message
    osm_cancel     - Cancel an I/O request that is being processed.
    osm_posted     - Thread is posted out of its I/O wait

  PRIVATE FUNCTIONS 

  EXAMPLES 

  NOTES

     The declarations in this file presume the types defined in oratypes.h.
     This include file should be included after oratypes.h or an equivalent
     declaration.

     It is intended that the code in the library be a thin layer that
     interfaces to a driver or some equivalent software layer. The code cannot
     call any of the routines in the Oracle kernel. It may not use shared
     memory to communicate between different execution threads. It must
     tolerate termination of an execution thread between any two instructions
     with no effect on any other execution thread. It must not change any
     signal handling.

     The library will be dynamically linked with the Oracle kernel. There
     could be multiple osmlib implementations simultaneously linked to the
     same Oracle kernel. Each library would provide access to a different
     set of disks.

  MODIFIED
     wbridge    01/30/03 - 30 char limit on failure group name and disk label
     wbridge    12/05/02 - remove atomic key change on I/O
     pbagal     11/22/02 - Introduce maxio size for read & write
     wbridge    09/11/02 - tweek comments
     wbridge    08/29/02 - remove registration and add other improvements
     wbridge    04/22/02 - interface is a reserved word on NT eliminate it.
     rlong      04/15/02 - Oracle Storage Manager merge
     wbridge    03/15/02 - eliminate hard tabs.
     wbridge    03/05/02 - better yet.
     wbridge    02/01/02 - improve comments.
     wbridge    01/11/02 - eliminate discovery returning an array of disks.
     wbridge    12/12/01 - make unique disk id into a string.
     wbridge    11/19/01 - add per I/O information.
     pbagal     07/03/01 - make osmlib.h aware of  osmlibdoc script.
     wbridge    06/27/01 - Merged wbridge_osmlib
     wbridge    06/21/01 - first revision.

*/

#ifndef OSMLIB_ORACLE
# define OSMLIB_ORACLE  

/*---------------------------------------------------------------------------*/
/*                    PUBLIC TYPES AND CONSTANTS                             */
/*---------------------------------------------------------------------------*/

/*
 * Instance identifier
 *
 * The very first call to osmlib, when an Oracle instance is being started,
 * negotiates the version of the library and verifies that the correct system
 * software is installed. This results in an instance identifier that is used
 * by all execution threads in an instance when they initialize their osmlib
 * state. This allows the library to know that a group of execution threads
 * are cooperating as an instance and will be sharing disk identifiers. The
 * contents of the identifier is opaque to Oracle. If the library does not
 * need an instance identifier this value can be zero.  */
typedef ub8 osm_iid;                                  /* instance identifier */

/*
 * osmlib context.
 * 
 * The osmlib routines cannot declare any static or global variables. In order
 * to allow them to keep state from one call to another a context may be
 * created by the initialization routine osm_init() and it will be passed in
 * to every other osmlib call. osm_init() may call malloc() to allocate this
 * context. Other osmlib routines may also call malloc as needed. All memory
 * allocated through malloc() must be freed through free() before osm_fini()
 * returns. A different context is used by each execution thread. The pointer
 * to the context is of type osm_ctx for this interface, but it is expected
 * that the library routines will cast it to their own type.  */
typedef void *osm_ctx;                                    /* context pointer */

/*
 * Error code
 *
 * Every osmlib call, except osm_version(), returns an error code. An error
 * code of zero indicates the call was successful. Any other error code
 * indicates that something went wrong. A negative error code means that a
 * software bug has been encountered - some parameter does not make sense
 * (e.g. an invalid pointer), or some osmlib internal structure has been
 * damaged. A negative error code will always result in Oracle signaling an
 * internal error. A positive error code indicates that there has been a
 * hardware failure or a user made a mistake. A disk I/O error or an invalid
 * disk name would generate a positive error code. The code is converted into
 * a meaningful message by osm_error(). Thus the meaning of an error code is
 * completely up to the osmlib implementation. Oracle only interprets the sign
 * of an error. In the descriptions of the routines that follow, examples are
 * given of possible errors. These examples are not a complete list of all
 * possible errors. There may be implementations where the example errors
 * could not occur. The examples are intended to aid understanding, not limit
 * the set of errors.  */
typedef sword osm_erc;                                      /* an error code */

/*
 * Disk name and attributes structure
 *
 * The name structure is used to describe a disk to Oracle. Discovery returns
 * name structures as its result. A name can be used to open a disk for use by
 * an Oracle instance. A copy of the name structure can be used by any
 * instance on the same node of a cluster to open the same disk. A name
 * discovered on one node is never passed to another node for opening. A name
 * structure also contains information about the disk that is useful for
 * adding it to a disk group. This simplifies administration since the
 * administrator will not have to enter these values. It also allows warnings
 * when disks with significantly different characteristics are added to the
 * same disk group.
 */
typedef struct osm_name osm_name;                /* disk name and attributes */

/* Size limits on strings. Limit value includes terminating null */
#define OSM_MAXPATH   256     /* maximum size of a null terminated path name */
#define OSM_MAXLABEL  31               /* maximum characters in a disk label */
#define OSM_MAXFGROUP 31       /* maximum characters in a failure group name */
#define OSM_MAXUDID   64           /* maximum characters in a unique disk ID */

struct osm_name
{
  uword    interface_osm_name;                /* osmlib interfaces supported */
  /* This field indicates what can be done with the disk through the OSM
   * interface. It is a bit mask with a bit set for every feature that this
   * disk supports. These same bits are also returned by osm_version() to
   * indicate the interfaces supported by the library. The library may support
   * an interface that some disks do not support even though the library is
   * capable of discovering the disks. For example some disks may not have the
   * ability to support key validation even though the library can issue the
   * keyed access commands to disks. The value in this field must never have a
   * bit set that was not set in the interface mask returned by
   * osm_version(). The following interface bits are defined. */
#define OSM_IO       0x0001 /* osmlib style I/O with per instance descriptor */
        /* The routines osm_open(), osm_close(), and osm_io() will be called
         * to read and write the contents of this disk.  If it is not set then
         * osmlib is only used for discovery, and the disks are accessed
         * through the normal operating system interfaces for
         * open/close/read/write. If this is not set the path name must be a
         * valid OS file name - OSM_OSNAME set. */
#define OSM_KEYVALID 0x0002               /* osmlib key validation supported */
        /* The disk supports the use of keys to validate I/O. The OSM_KEYCHK
         * and OSM_FENCE flags may be set in the operation field of I/O
         * control blocks passed to osm_io(). The operations to get and set
         * disk keys and fence keys are supported. The number of disk keys and
         * the number of fence keys must not be zero. If this flag is set then
         * OSM_IO must be set. */
#define OSM_UDID     0x0004           /* Unique Disk ID's reported for disks */
        /* The unique disk id field udid_osm_name contains a value that can be
         * used to uniquely identify this disk with respect to any other disk
         * ever manufactured. The same unique id will be returned by any
         * discovery of this disk by any node. */
#define OSM_FGROUP   0x0008      /* osmlib failure groups reported for disks */
        /* A failure group name has been provided for this disk in
         * fgroup_osm_name. The user does not need to enter the name when
         * adding the disk. */
#define OSM_QOS      0x0010                   /* Quality of service reported */
        /* The redundancy and maximum transfer rate are provided for this disk
         * in mirror_osm_name, parity_osm_name, and speed_osm_name;. */
#define OSM_OSNAME   0x0020             /* path name is a valid OS file name */
        /* A valid OS file name is provided in path_osm_name. The name may be
         * used by any process on this node to access the contents of this
         * disk through the normal operating system I/O interface. Oracle will
         * only use this name to access the disk if OSM_IO is not set. This
         * must be set if OSM_IO is not set. Even if this bit is not set,
         * path_osm_name may contain a non-null string, but it may be in some
         * other file name space. */
#define OSM_ICOPY    0x0040                       /* internal copy supported */
        /* This is set if serverless copies within this disk are supported. A
         * read from one area of the disk can be linked to a write to another
         * area of the same disk. If this flag is set then OSM_IO must be
         * set. */
#define OSM_XCOPY    0x0080                       /* external copy supported */
        /* This is set if serverless copies are supported between disks
         * accessed through this library.  A read from this disk can be linked
         * to a write to any other disk with OSM_XCOPY set. Note that
         * OSM_XCOPY implies OSM_ICOPY. */
#define OSM_ABNVALID   0x0100     /* Virtual block number checking supported */
        /* This is set if the application block number validators can be used
         * to validate block numbers in the data. The disk can be told about
         * the first application block number, application block size, and the
         * location of the number within the block to validate that every
         * application block number is correct in the data. If this flag is
         * set then OSM_IO must be set. */
#define OSM_XORVALID 0x0200                 /* Data XOR validation supported */
        /* This is set if the disk can validate an XOR of the data per logical
         * block matches a value set in the I/O command. If the data does not
         * match on a write then an error is reported and preferably the disk
         * will not be updated. This is to catch errors in transmitting the
         * data to the disk. If this flag is set then OSM_IO must be set. */
#define OSM_TAGVALID 0x0400            /* Partition tag validation supported */
        /* This is set if the disk can keep partition tags and validate them
         * on every write. There is a command to set a tag and a range of
         * blocks of an entry in the partition table. Any write to a block in
         * a tagged partition must contain the correct tag in the command.
         * This is used to assign areas of a disk to a particular application,
         * and to verify that writes only come from the correct application.
         * This prevents a stray write by one application from damaging data
         * of another application. If this flag is set then OSM_IO must be
         * set. */
  
  oratext label_osm_name[OSM_MAXLABEL];                        /* Disk label */
  /* This is a human readable label associated with the disk. It is a null
   * terminated string. It is the default for the osmlib disk name when the
   * disk is added to a disk group. It should be unique for all the disks in a
   * disk group. Changing the label will not change the osmlib disk name. If
   * the library cannot provide a label this may be the null string. */

  oratext udid_osm_name[OSM_MAXUDID];                    /* Unique Disk ID */
  /* If OSM_UDID is set then this field contains a world wide unique id for
   * this disk as a null terminated string. It could be a manufacturer and
   * serial number or it could be a fiber channel world wide name. It is used
   * to detect two paths to the same disk so that the disk is not treated as
   * if it was two disks. The id is not persistently stored anywhere. There
   * would be no problem if the same contents appeared under a different
   * unique disk id while the disk group was dismounted or the disk was
   * offline. A remote mirror of a disk group could be activated even though
   * all the disks would have a different unique disk id. The unique disk id
   * must not change while a disk is open. If a unique id exists then it must
   * be a valid discovery string for passing to osm_discover().  The call to
   * osm_discover() may be made on any node in cluster, and it will only
   * discover the one disk described by this osm_name (or no disks if this
   * disk is no longer available). */
  
  oratext path_osm_name[OSM_MAXPATH];                    /* Access path name */
  /* This is a node local name for accessing the disk. It would commonly be
   * the path name for a Unix raw device. However it could be a name that is
   * only meaningful to the osmlib library. If it exists then it must also be
   * a valid discovery string for passing to osm_discover(). In a cluster it
   * will only be used on the same node that discovered this name. It will
   * refer to the same disk if used by any process on the same node. If
   * OSM_OSNAME is set, it is a standard OS file name and the standard
   * operating system I/O interface will access the same disk that is accessed
   * through the osmlib library.  If OSM_UDID is set then this could be the
   * null string and the unique disk id is used for finding the disk. */

  oratext fgroup_osm_name[OSM_MAXFGROUP];              /* Failure group name */
  /* If OSM_FGROUP is set then this contains a null terminated string that
   * names the failure group containing this disk. If OSM_FGROUP is not set
   * then this contains the null string. The failure group is used to select
   * disks to mirror extents of files. When selecting two disks to keep
   * mirrored copies of an extent, the disks are chosen from different failure
   * groups. Two disks are in the same failure group if they share a piece of
   * hardware who's failure would make the disks unavailable, and the failure
   * must be survived without loss of data. Thus the partitioning of disks
   * into failure groups depends on the reliability requirements of the
   * application and the size of the system. A highly redundant storage
   * network may be reliable enough that its failure is not considered as a
   * criteria for determining failure groups. A system with only one disk
   * controller would not consider disks on the same controller as being in
   * the same failure group. However a system with 8 disk controllers would
   * likely consider the disks as being in 8 named failure groups. */

  ub4     blksz_osm_name;                 /* physical block size of the disk */
  /* This is the physical sector size of the disk in bytes. All I/O's to the
   * disk are described in physical sectors. This must be a power of 2. An
   * ideal value would be 4096, but most disks are formatted with 512 byte
   * sectors. */
  
  ub4     maxio_osm_name;                      /* maximum I/O size in blocks */
  /* This is largest value allowed in rcount_osm_ioc for a read or write
   * command. Oracle will never request a larger transfer. This should be a
   * power of two. The library will not break large requests into smaller
   * requests. The entire data buffer will be associated with a single command
   * to the disk. This parameter allows Oracle to better track I/O resource
   * consumption. It also ensures that a host software failure will not result
   * in partial writes.  Mirrored writes to two disks are very unlikely to
   * result in both I/O's being partially completed.  There will be a
   * significant performance problem if this value is much less than 512K
   * bytes. */

  ub4     max_abs_osm_name;      /* Maximum Application Block Size in blocks */
  /* This is the maximum supported value for abs_osm_ioc. The disk may have a
   * limited amount of buffering that it can use to hold data while gathering
   * an application block for calculating the XOR of the data and preventing
   * fractured blocks. If a larger application block size is put in
   * abs_osm_ioc then the validation may not be done, or some of the data may
   * be written to disk before the entire application block is received. Note
   * that exceeding this limit should not result in an error. Oracle uses
   * blocks of up to 32K bytes, so it would be best to have this be at least
   * that large. Larger values may be useful for data that has not been forced
   * to XOR to a particular value. To validate the XOR of this kind of data
   * the application block size is set to the size of the transfer, and the
   * XOR of the data is stored in xor_osm_ioc. With this algorithm
   * max_abs_osm_name becomes the maximum transfer size. */
  
  ub8     size_osm_name;                              /* disk size in blocks */
  /* This is the size of the disk in physical blocks. The size of a disk could
   * change between discoveries. This would happen if the disk is actually a
   * virtual disk. The amount of space used by Oracle does not change just
   * because the amount discovered changes. An administrative command can
   * change the amount of space used by Oracle. */
   
  ub4     keys_osm_name;                    /* number of disk keys supported */
  /* If OSM_KEYVALID is set then this is the number of disk keys that the disk
   * can maintain. In the year 2000 there should be one key for every megabyte
   * of storage. When disks get much larger it may make sense to have a key
   * for every 2 or 4 megabytes. The number of keys should be user selectable
   * when the disk is formatted, just like the sector size. The key values
   * must be initialized to zero when the disk is formatted, or if the current
   * values are lost. If OSM_KEYVALID is not set then this field must be
   * zero. Just as the amount of space can change for a virtual disk, the
   * number of disk keys can change too. A disk can become unusable if the
   * number of disk keys does not change with the size of the disk. */
  
  ub4     fences_osm_name;                 /* number of fence keys supported */
  /* If OSM_KEYVALID is set then this is the number of fence keys that the
   * disk can maintain. A disk needs to have one key for every Oracle instance
   * that can access the disk. A large cluster with several databases could
   * have a few thousand Oracle instances.  The number of fence keys should be
   * selectable when formatting the disk. The key values must be initialized
   * to zero when the disk is formatted, or if the current values are lost. If
   * OSM_KEYVALID is not set then this field must be zero. */

  ub2     tags_osm_name;       /* maximum number of partition tags supported */
  /* If OSM_TAGVALID is set then this is the number of partition tags that the
   * disk can maintain. 32 tags should be enough for any expected use. When a
   * disk is formatted all tags are cleared. This is equivalent to setting all
   * partitions to have a tag of zero. Oracle only needs one tagged
   * partition. */
  
  ub2     mirror_osm_name;  /* number of mirrored disks underlying this disk */
  ub2     parity_osm_name;/* number of Data blocks covered by a parity block */
  ub2     speed_osm_name;       /* Max transfer rate in megabytes per second */
  /* These three fields are set if OSM_QOS is set. They are used to give
   * warning messages if disks of significantly different characteristics are
   * combined into one disk group. Since a disk cannot be both mirrored and
   * parity protected, either mirror_osm_name or parity_osm_name must be
   * zero. A simple unmirrored disk will have mirror_osm_name set to one and
   * parity_osm_name set to zero. */

  ub8    reserved_osm_name;                                      /* Reserved */
  /* This is an 8 byte field which is never modified by Oracle.  It may be
   * used by osmlib to find its internal data structure associated with this
   * disk name.  */
};

/*
 * Open disk handle
 *
 * A disk handle is used to indicate which disk is the target of an I/O
 * operation. An osm_open() call is used to allocate a handle for a disk that
 * was discovered through osm_discover(). The same handle can be used by any
 * thread of execution that called osm_init() with the same instance id as the
 * caller to osm_open(). Thus one process in an instance can open a disk and
 * share the handle with all the processes in the instance. A handle is only
 * valid on the node where it is created. The same disk may be opened multiple
 * times. The handle does not have to be the same for each open, but it must
 * access the same media.
 */
typedef ub8 osm_handle;

/*
 * Partition tag
 *
 * If OSM_TAGVALID is set in interface_osm_name then the disk supports a
 * partition table with a tag for each partition. Blocks in tagged partitions
 * can only be written by write commands that contain the correct tag. This
 * structure describes one partition. These are passed as data to OSM_SETTAG
 * commands to modify one or more entries in the partition table. The data
 * returned from OSM_GETTAG is an array of these structures. The disk has a
 * persistent array of these structures that is of length tags_osm_name. The
 * entries in the array do not have to be ordered by block number. A block
 * that is not within any parition must be written with a command that does
 * not have a tag. If two partitions overlap then the blocks in the overlap
 * may be written with either tag. */
typedef struct osm_tag osm_tag;                  /* partition tag descriptor */
struct osm_tag
{
  ub8     first_osm_tag;            /* first physical block in the partition */
  ub8     size_osm_tag;          /* size of the partition in physical blocks */
  ub8     value_osm_tag;                   /* tag expected in write commands */
};

/*
 * Check data and results
 *
 * If OSM_KEYVALID is set in interface_osm_name then an I/O operation can have
 * check data to be validated when doing the I/O. This structure contains the
 * the data for making the check and a place to return key values if the
 * check fails. This structure is logically part of the I/O control block,
 * but it is in a separate structure to avoid the overhead when keys are
 * not used.
 */
typedef struct osm_check osm_check;                /* check data and results */
struct osm_check
{
  ub4     fence_num_osm_check;                      /* fence key to validate */
  /* If OSM_FENCE is set in operation_osm_ioc then this is the number of the
   * fence key to compare with fence_value_osm_check.  It must be less than
   * fences_osm_name. The first key is number zero. */
  
  ub4     fence_value_osm_check;    /* value to compare with fence key value */
  /* If OSM_FENCE is set in operation_osm_ioc then this is the value to compare
   * with the value stored in the disk. */

  ub4     key_num_osm_check;                         /* disk key to validate */
  /* If OSM_KEYCHK is set in operation_osm_ioc then this is the number of the
   * disk key to test.  It must be less than keys_osm_name. The first key is
   * number zero. */
  
  ub4    error_fence_osm_check;      /* actual fence key value if fenced out */
  /* If the operation completes with status bit OSM_FENCED set, then the
   * actual value of the fence key is returned here. This is the value that
   * was compared with fence_value_osm_check and found not to match. It is
   * possible that the value has changed since the operation failed so that
   * this is no longer the current value. It is possible that the operation
   * did succeed once, but was fenced on a retry. A value of zero indicates
   * that the disk has lost its key values and they must be reloaded. */

  ub8     key_mask_osm_check;                 /* mask of key bits to compare */
  /* If OSM_KEYCHK is set in operation_osm_ioc then this is used to determine
   * which bits are compared when validating the disk key. Only the bits that
   * are 1 in this mask will be tested when comparing the value on disk with
   * key_value_osm_check. Thus a mask of zero causes any value to be a
   * match. */

  ub8     key_value_osm_check;     /* value to compare with masked key value */
  /* If OSM_KEYCHK is set in operation_osm_ioc then this value is compared with
   * the value in the disk key, subject to masking. */
  
  ub8    error_key_osm_check;       /* actual disk key value if key mismatch */
  /* If the operation completes with status bit OSM_BADKEY set, then the
   * actual value of the disk key is returned here. This is the value that was
   * anded with key_mask_osm_check and compared with key_value_osm_check and
   * found not to match. This is the whole key value before masking with
   * key_mask_osm_check. It is possible that the value has changed since the
   * operation failed so that this is no longer the current value. It is also
   * possible that this is the result of a retry after a successful operation.
   * A value of zero indicates that the disk has lost its key values and they
   * must be reloaded. */
};



/*
 * I/O control block
 *
 * This structure is used to control a single I/O request. It is allocated and
 * initialized by the Oracle kernel then passed to osm_io(). Oracle does not
 * modify the contents of the memory until osm_io() returns a pointer to the
 * block with an indication that it is done with the request. The block will
 * then describe the results of the request. Oracle can examine the status
 * before the request is complete, but there is no guarantee that it will be
 * updated with the latest value. When the request is complete the memory may
 * be made free or used for another I/O request. The structure may be in
 * thread private or shared memory. If a thread of execution terminates while
 * osmlib owns a control block then the contents of the control block are
 * meaningless and any I/O that may have been associated with the control
 * block must be terminated by osmlib before the thread is reported as dead by
 * the operating system.  Thus a control block in shared memory may be
 * reclaimed when its execution thread dies even though a status of free was
 * never set.  */
typedef struct osm_ioc   osm_ioc;                       /* I/O control block */
struct osm_ioc {                                        /* I/O control block */
  ub4       ccount_osm_ioc;                               /* Completed count */
  /* If OSM_PARTIAL is set in the status, then ccount_osm_ioc is the number of
   * blocks that were successfully transferred starting at the beginning of
   * the request. This value is only valid if OSM_PARTIAL is set in the
   * status.  Some data may be successfully transferred even if there is an
   * error later on.  This information is particularly useful for reads of log
   * files where all redo data is needed up to the first piece lost. Note that
   * a failure on the 3rd block of a 4 block transfer gives a completion count
   * of 2 blocks even if the fourth block was successfully transferred. An
   * error will be set to indicate the reason that the request was not
   * completed. */

  osm_erc    error_osm_ioc;                                    /* Error code */
  /* This field is zero at completion if the I/O is successful. If there were
   * any problems then OSM_ERROR will be set in the status and this field
   * contains an error code indicating the reason the request failed. The
   * error code could be negative if buffer_osm_ioc is bad or a byte count is
   * too big or not a multiple of the physical block size. Errors involving
   * hardware failures or permissions are positive. A disk key or fence key
   * mismatch is not an error, and does not store an error code in
   * error_osm_ioc. Errors are not returned unless all reasonable attempts
   * to retry have failed. OSM will not retry the operation as part of error
   * recovery. */

  osm_erc   warn_osm_ioc;                              /* Warning error code */
  /* This field is zero if the I/O completed normally. If the I/O was
   * successful, but future I/O's may fail then OSM_WARN can be set in the
   * status field and this field can contain a message code. A warning can be
   * used to indicate situations such as over temperature, or excessive
   * recoverable errors indicating the disk will soon fail. The message code
   * will be converted to a string via osm_iowarn and displayed to an
   * administrator. In all other ways the system will continue to operate
   * normally. It is possible to get both a warning and an error on the same
   * request. The warning may or may not be related to the cause of the
   * error. */
  
  ub4    elaptime_osm_ioc;                                   /* Elapsed Time */
  /* This field is set to the time elapsed between setting the OSM_SUBMITTED
   * status flag and setting the OSM_COMPLETED flag, as a measure of the I/O
   * response time.  The elapsed time is reported in microseconds, but does
   * not need to be that accurate. */

  ub2     status_osm_ioc;                           /* status of the request */
  /* The status is a set of flags indicating what has happened to the request.
   * Oracle initializes the status to zero before passing the block to
   * osm_io().  As the status of the request changes, osmlib will set one or
   * more flags.  Flags are never cleared by osmlib. Some flags may never be
   * set. The following flags are defined. */
#define OSM_BUSY         0x0001                       /* too busy to process */
        /* This status indicates that it was not possible to submit the I/O
         * because the operating system is out of the resources necessary to
         * process a request.  For example it may not be possible to allocate
         * memory for a control structure to process the I/O. The statuses
         * OSM_FREE and OSM_ERROR will always be set when this is set. This
         * status will be set before the call to submit the I/O returns. The
         * request will not be returned as completed since that may require
         * some osmlib resource to keep track of the request. Oracle will check
         * every I/O control block for this flag immediately after submitting
         * the request. There is always an error code associated with this
         * request so that the name of the resource can be reported to the
         * user. This should be a very rare situation indicating a serious
         * misconfiguration of the I/O system. This is not intended as a flow
         * control mechanism. */
#define OSM_SUBMITTED    0x0002          /* request submitted for processing */
        /* This status is set when the call to start the I/O has been
         * successful. It is set before the call to submit the I/O returns. It
         * is always set if OSM_BUSY is not set and osm_io() does not report an
         * error. The request will eventually be returned as completed by a
         * call to osm_io(), unless the thread of execution terminates. */
#define OSM_COMPLETED    0x0004                         /* request completed */
        /* This status is set when the device and the device I/O routine have
         * finished processing the request.  The completion count, elapsed
         * time, and error code will be stored before this flag is set. This
         * does not mean that the control block has already been returned as
         * completed. It does mean that an osm_io() call will return this block
         * immediately if there is room in the completion list. This status
         * might not be set even though the I/O has completed. In some
         * implementations it may require an osm_io() call to modify the
         * status. Thus Oracle cannot loop checking for this bit before
         * calling osm_io() to get the completion. */
#define OSM_FREE         0x0008                            /* memory is free */
        /* This status indicates that the control block has been returned to
         * the caller. osmlib may not access this block again unless it is
         * passed in for another request. Oracle may delete or reuse the
         * memory containing the I/O control block. This will always be set
         * when osm_io() has returned the block as completed. */
#define OSM_CANCELLED    0x0010                         /* request cancelled */
        /* This status is set when osm_cancel() is called to terminate the
         * request. The I/O may be partially done, completed, still in
         * progress or not started. The completion count and elapsed time
         * might not be accurate even if the OSM_COMPLETED status has been
         * set. This is set even if the completed status was set when
         * osm_cancel() was called. */
#define OSM_ERROR        0x0020              /* request failed with an error */
        /* This status is set if there was an error while processing the
         * request. The error code can be found in error_osm_ioc. This osm_ioc
         * must be passed to osm_ioerror() to interpret the error code. */
#define OSM_WARN         0x0040                 /* a future request may fail */
        /* This status is set if there was a warning while processing the
         * request. The message code for the warning can be found in
         * warn_osm_ioc. This osm_ioc must be passed to osm_iowarn() to
         * interpret the message code. A warning does not imply the operation
         * was successful or failed. */
#define OSM_PARTIAL      0x0080                   /* only a partial transfer */
        /* This status is set when only a portion of the requested data was
         * transferred.  When this is set the completion count is different
         * from the request count. OSM_ERROR will always be set when
         * OSM_PARTIAL is set, and the error code will report why all the data
         * was not transferred. */
#define OSM_BADKEY       0x0100                         /* disk key mismatch */
        /* This status bit is set when the operation failed due to a disk key
         * mismatch. This can only happen if the OSM_KEYCHK flag was set in
         * the operation. The key value that was in the disk is returned in
         * error_key_osm_check. A key mismatch is not an error, so OSM_ERROR
         * must not be set if OSM_BADKEY is set. This status does mean the
         * operation is complete, so OSM_COMPLETED will be set. Note that a
         * key mismatch does not mean that the operation did not execute. It
         * is possible that the operation completed successfully, but a bus
         * reset lost the completion status and the operation was reissued.
         * The reissued operation may encounter a different key value due to a
         * setkey operation on the key. The new key value may cause the key
         * mismatch even though the operation succeeded when first issued. */
#define OSM_FENCED       0x0200      /* I/O was not allowed by the fence key */
        /* This status bit is set when the operation failed due to a fence key
         * mismatch. This can only happen if the OSM_FENCE flag was set in the
         * operation. The key value that was in the disk is returned in
         * error_fence_osm_check. A key mismatch is not an error, so OSM_ERROR
         * must not be set if OSM_FENCED is set. This status does mean the
         * operation is complete, so OSM_COMPLETED will be set. As with a disk
         * key mismatch, the operation may have succeeded but the retry was
         * fenced off. */

  ub2     flags_osm_ioc;   /* flags set by Oracle describing the i/o request */
  /* These flags give additional information about the request and/or
   * modify the opcode to require additional checking. */
#define OSM_KEYCHK      0x0001           /* validate disk key with operation */
        /* If OSM_KEYVALID is set in interface_osm_name then this may be set
         * to check a disk key before performing the operation.  The key to
         * validate is specified by key_num_osm_check. It is always less than
         * keys_osm_name. If the key value stored in the disk is key_disk then
         * the operation is allowed if ((key_disk ^ key_value_osm_check) &
         * key_mask_osm_check) == 0 Thus a key_mask_osm_check of zero allows
         * any key_disk to be valid.  A key mismatch is not an error even
         * though it prevents the operation from being executed. A disk key
         * mismatch will set OSM_BADKEY in status_osm_ioc, and return the
         * entire disk key value in error_key_osm_check. The intent is to
         * store metadata in the disk keys and have it validated on every I/O
         * that depends on it. Thus cached copies of the metadata in other
         * hosts on the SAN can be invalidated through the disk itself.  If a
         * disk loses its disk key values, the values will be set to zero by
         * the disk. A key of zero will always fail any key check used by
         * OSM. */
#define OSM_FENCE       0x0002          /* validate fence key with operation */
        /* If OSM_KEYVALID is set in interface_osm_name then this may be set to
         * atomically check a fence key along with the operation. The key to
         * validate is specified by fence_num_osm_check. It is always less than
         * fences_osm_name. If the fence key value stored by the disk is equal
         * to fence_value_osm_check then the operation is allowed. If it does
         * not match then the operation is prevented, OSM_FENCED is set in
         * status_osm_ioc, and error_fence_osm_check will contain the key from
         * the disk. This provides a mechanism to fence off accesses from an
         * instance that should be dead. It provides better granularity than
         * fencing at the node level because it allows one application to be
         * fenced without affecting other applications running running on the
         * same node sharing the same disk.  If a disk loses its fence key
         * values, the values will be set to zero. A key of zero will always
         * fail any fence key check used by OSM. */
#define OSM_BATCHED     0x0004            /* request will be reaped in batch */
        /* This flag indicates that this I/O request will be reaped in a 
         * batch and not as a single I/O request. The implementor can use this 
         * hint to determine how they wait for this I/O completion. */
#define OSM_ABNCHK   0x0008     /* request includes application block number */
        /* If this flag is set then the application block number fields
         * (abs_osm_ioc, abn_offset_osm_ioc, abn_osm_ioc, abn_mask_osm_ioc)
         * can be used by the storage device to verify that the correct data
         * buffer has been transfered on a disk write. The application block
         * validators are sent in the command block along with the disk
         * address (e.g., SCSI CDB). The data is commonly sent later via a
         * different mechanism. If validation fails the library should do at
         * least one retry. However a negative error should be returned on a
         * write where the disk detected bad application block numbers, and
         * the data buffer passed in by OSM contained incorrect application
         * block numbers.  A write that fails the validation returns an error
         * and does not update the disk with the block that failed. Writes of
         * multiple application blocks may update some blocks that passed the
         * verification before encountering a block that fails. This flag will
         * only be set on writes. */
#define OSM_XORCHK      0x0010               /* request includes XOR of data */
        /* This flag indicates that a 16 bit wide XOR of the data in the
         * application blocks is stored in xor_osm_ioc. Every application
         * block in a multiblock transfer will XOR to this same value. If the
         * application cannot make every block XOR to the same value, then OSM
         * may set the application block size (abs_osm_ioc) to the size of the
         * transfer (rcount_osm_ioc) and set xor_osm_ioc to the XOR of all the
         * data. On a write this can be used to verify that the data has not
         * been damaged on its way to the disk. A write that fails to XOR to
         * the correct value returns an error and preferably does not update
         * the disk. Some implementations may not be able to detect the error
         * before modifying the persistent storage. While this defeats the
         * major advantage of this feature, it is still better than no
         * checking at all. This may also happen if abs_osm_ioc is greater
         * than maxabs_osm_name. Writes of multiple application blocks may
         * update some blocks that passed the verification before encountering
         * a block that fails.  This flag will only be set on writes. */
#define OSM_TAGCHK      0x0020             /* request includes partition tag */
        /* This flag indicates that the expected tag for the partion is in
         * tag_osm_ioc. The blocks being written must be assocaited with the
         * same tag value. If the tag is wrong then the write must not update
         * the disk, and an error is returned for the write. If this flag is
         * not set on a write then the blocks must not be within a tagged
         * partition. This flag will only be set on writes. */
#define OSM_OK_FRACTURE 0x0040     /* fractured writes will not corrupt data */
        /* This flag is set if an I/O error on this write means the data being
         * written will never be read. For example, initializating a file at
         * creation will fail the creation if there is an error. If this flag
         * is not set then it is best to avoid partial writes of an
         * application block as given by abs_osm_ioc. The disk should buffer
         * all the data for an application block before making any of it
         * persistent. Recovery of a partially written application block is
         * more expensive than an unwritten block. There is always the
         * possibility of a disk failure resulting in a partial write, but by
         * buffering all the data from the host before writing, it is less
         * likely a host failure will cause a partial write. If abs_osm_ioc is
         * zero or greater than maxabs_osm_name then the disk will not be able
         * to provide this feature.  This flag will only be set on writes. */
  
  ub1    operation_osm_ioc;                           /* which I/O operation */
  /* This field defines what I/O operation is being requested. Usually this is
   * either a read or a write. However there are some other less frequently
   * used operations as well.  The supported codes are listed below. */
#define OSM_NOOP        0x00                         /* no-op to pass a hint */
        /* This operation does not do any transfer. It can be used as a means
         * of allowing Oracle to indicate it is still interested in the
         * contents of a block. The memory address in the request might not be
         * a valid address. It is commonly zero. This operation could be used
         * to keep blocks in the disk cache when they are hot read-only blocks
         * in the Oracle cache so that restarting the Oracle instance would be
         * faster. This operation will not be used in Oracle 10. It is only
         * defined in this interface for compatibility with future releases of
         * Oracle. */
#define OSM_READ        0x01                          /* Read data from disk */
        /* Transfer disk blocks to memory from disk. This must read the latest
         * version written to the disk and acknowledged as complete. However
         * if there is a write in progress to the same blocks, which has been
         * started but not acknowledged as complete, then either the new
         * version, the old version, or a mix of blocks from the two versions
         * may be returned. */
#define OSM_WRITE       0x02                           /* write data to disk */
        /* Transfer disk blocks from memory to disk. When the OSM_FREE flag is
         * set in the status and the write is successful, it is guaranteed that
         * any future read will see this data or get an error. This must be
         * guaranteed even if there is a failure such as the loss of a highly
         * reliable NVRAM cache. Oracle will never issue two simultaneous
         * writes to the same blocks except in one very unlikely case where
         * the two writes contain the same data. (Two readers simultaneously
         * repairing the same corrupt block.) Thus mixing the data from the
         * two writes would not be a problem. */
#define OSM_COPY        0x03       /* copy data from one location to another */
        /* Copy blocks from one disk location to others. The data does not
         * appear in a buffer so this is a serverless copy. This I/O control
         * block describes the data to read. The link_osm_ioc field points at
         * a list of osm_ioc structures to define the writes. The list is only
         * used to get the parameters for doing the writes.  They are not
         * treated as independent I/O's. They do not have their status updated
         * except for a key mismatch. They must have an opcode of OSM_WRITE
         * and a null buffer pointers. The last write command in the list has
         * a null link pointer. If a write is to the same disk as the read
         * then OSM_ICOPY must have been set in interface_osm_name.  If a
         * write is to a different disk then OSM_XCOPY must have been set in
         * interface_osm_name for both disks. */
#define OSM_GETKEY      0x04           /* get value of one or more disk keys */
        /* Transfer disk key values to memory from disk key storage. The
         * number of the first key to be gotten is in first_osm_ioc. The data
         * buffer pointed to by buffer_osm_ioc contains an array of
         * rcount_osm_ioc 8 byte disk key buffers. If there are any setkey
         * operations in progress, then either the before or after version of
         * the key may be returned. There is no need to return a consistent
         * version of multiple keys. This is mostly used for debugging and
         * statistics. */
#define OSM_SETKEY      0x05           /* set value of one or more disk keys */
        /* Transfer disk key values from memory to disk key storage. The
         * number of the first key to be set is in first_osm_ioc. The data
         * buffer pointed to by buffer_osm_ioc contains an array of
         * rcount_osm_ioc 8 byte disk key values. If there are any operations
         * in progress that were validated by the previous value in any of the
         * keys being set, then the previous operation must completed before
         * this setkey operation is marked as successfully completed. New
         * operations validated by the new key values may proceed and even
         * complete before this setkey operation completes. This operation
         * will be coordinated by Oracle so there should not be conflicting
         * set key operations. If there are conflicting operations then the
         * results are undefined. In particular this mechanism cannot be used
         * as a test and set by having it validated by the old value in the
         * key. When a disk is first formatted, or if the key values are lost,
         * the keys must be initialized to zero. OSM uses zero as an
         * indication that it must reload the keys. OSM will not use zero as a
         * valid key value, but setting a key to zero through this operation
         * should be allowed. If a setkey command fails with an error, then
         * the key value may be either the old value, the new value, or
         * zero. */
#define OSM_GETFENCE    0x06          /* get value of one or more fence keys */
        /* Transfer fence key values to memory from fence key storage. The
         * number of the first key to be gotten is in first_osm_ioc.  The data
         * buffer pointed to by buffer_osm_ioc contains an array of
         * rcount_osm_ioc 4 byte fence key buffers. If there are any setfence
         * operations in progress that affect the key values, then either the
         * before or after version of the key may be returned. There is no
         * need to return a consistent version of multiple keys. This is
         * mostly used for debugging and statistics. */
#define OSM_SETFENCE    0x07          /* set value of one or more fence keys */
        /* Transfer fence key values from memory to fence key storage. The
         * number of the first key to be set is in first_osm_ioc. The data
         * buffer pointed to by buffer_osm_ioc contains an array of
         * rcount_osm_ioc 4 byte fence key values.  If there are any
         * operations in progress that were validated by the previous value in
         * any of the fence keys being set, then the previous operation must
         * completed before this setfence operation is marked as successfully
         * completed. New operations validated by the new fence key values may
         * proceed and even complete before this setfence operation
         * completes. This operation will be used to initialize keys when a
         * disk is being brought into service, set the key of an instance when
         * it starts up, and fence out an instance that should be dead. Except
         * for initialization, this operation is combined with OSM_FENCE to
         * prevent conflicting set fence operations. If there are conflicting
         * operations then the results are undefined. When a disk is first
         * formatted, or if the key values are lost, the keys must be
         * initialized to zero.  OSM will not use zero as a valid key value,
         * but setting a key to zero through this operation should be
         * allowed. If a setfence command fails with an error, then the fence
         * key value may be either the old value, the new value, or zero. */
#define OSM_GETTAG      0x08            /* get tag of one or more partitions */
        /* Transfer partitions tags to memory from the persistent partition
         * table. The number of the first partition to get is in
         * first_osm_ioc. The data buffer pointed to by buffer_osm_ioc
         * contains an array of rcount_osm_ioc osm_tag buffers to hold the
         * partitions. This is only used for displaying the table to
         * administrators so there are no consistency issues with a
         * simultaneous update of the partition tags. */
#define OSM_SETTAG      0x10            /* set tag of one or more partitions */
        /* Transfer partitions tags from memory to the persistent partition
         * table. The number of the first partition to set is in
         * first_osm_ioc. The data buffer pointed to by buffer_osm_ioc
         * contains an array of rcount_osm_ioc osm_tag buffers containing the
         * new value for the partitions. Active writes submitted before the
         * settag do not have to be validated against the new partition table,
         * but it would be OK to validate them. Any writes issued after this
         * command successfully completes must be validated against the
         * tag. The partition table must be as persistent as a normal disk
         * block. Updates are infrequent and are expected to perform no better
         * than a low priority disk write. If a disk is remotely mirrored, then
         * the partition table on the remote mirror must be updated as if it
         * was a disk block. */

           
  ub1   priority_osm_ioc;                                        /* Priority */
  /* This field is a number, between 0 and 7, indicating the relative
   * importance of this I/O.  Priority 0 requests are processed ahead of all
   * other requests, possibly starving other priority classes.  For priorities
   * in the range from 1 to 7, the lower numbered priority requests get more
   * service than the higher numbered, but all requests are guaranteed to be
   * processed (no starvation). The disk is free to reorder requests of the
   * same priority to improve utilization of the disk. Even requests of
   * different priority may be serviced in reverse priority order. Ignoring
   * priority will not affect correctness, but may have an adverse affect on
   * performance. */

  ub2     hint_osm_ioc;                                      /* caching hint */
  /* This fields contain a hint giving the reason that the I/O was
   * issued. This can be used to select a caching policy in the disk for the
   * data in this request. Converting an I/O reason into a caching policy
   * depends heavily on the size of the cache relative to the size of the
   * disks. It is not expected that storage vendors understand all the
   * subtleties of Oracle I/O. It is sufficient to provide a set of caching
   * policies and a means of associating a policy with a hint value. Note that
   * in the future there may be additional reasons added to the ones currently
   * defined. Thus it must not be an error for this hint field to have an
   * undefined value. Ideally the cache in the disk should have a means of
   * assigning a policy to the new hint values. */
#define OSM_HINT_UNKNOWN     0                /* reason for I/O is not known */
        /* The code issuing this I/O has not provided a hint. */
#define OSM_HINT_LOG         1                   /* normal online log access */
        /* This is a write of the online redo log to commit a transaction or
         * a read of the log for archiving. It may also be a write of the log
         * header during a log switch. This is also used when a standby
         * database is accessing a standby log. */
#define OSM_HINT_LOG_OTHER   2                /* unusual online log accesses */
        /* On a write this is initializing an online log for later use. On a
         * read the online log is being read for recovery to apply it. */
#define OSM_HINT_DATA_CACHE  3       /* Normal datafile I/O to buffer cache  */
        /* The block will remain in the Oracle buffer cache after the I/O. If
         * this is a write then the write is to advance the checkpoint. If this
         * is a read then a buffer is being created in the cache for this
         * block. */
#define OSM_HINT_DATA_FLUSH  4           /* Block will be flushed from cache */
        /* The I/O is for a buffer in the cache, but the buffer will be 
         * discarded shortly after the I/O. If this is a write then the
         * buffer has aged out of the cache and is still dirty, or the block
         * is being taken offline. On a read, this hint means that as soon as
         * the buffer is used it will be quickly aged out of the cache, unless
         * it is used again. This happens on table scans into the cache. */
#define OSM_HINT_DATA_SEQ    5                   /* datafile sequential scan */
        /* The I/O is to a large process private buffer. For a write this is
         * a direct path operation to create or bulk load a table. For a read
         * this is a parallel query. This is *NOT* a hint that the next I/O
         * is going to go to the next location on disk. */
#define OSM_HINT_DATA_BACKUP 6            /* Backup or restore of a datafile */
        /* If this is a read then the data is going to be written to another 
         * file or tape to become a backup. If this is a write then the
         * datafile is being restored from a backup. This hint is also used
         * when initializing space in a datafile before it is used. */
#define OSM_HINT_DATA_HEADER 7                            /* Datafile header */
        /* This is a read or write of a datafile header. All file headers of
         * a database are read then written at database open. All file headers 
         * are read and rewritten on a regular basis while the database is
         * open. This happens roughly every log switch. */
#define OSM_HINT_SORT        8                     /* intermediate sort data */
        /* This is writing or reading of intermediate results during a sort.
         * Note that the data must be persistent even after it is read. A
         * subsequent read must return the same data even though this data is
         * usually not reread. */
#define OSM_HINT_CONTROL     9            /* Control file or other misc file */
        /* This is an access to either the database controlfile or some other
         * small file used to keep control information. This is used when
         * accessing the persistent parameters file (spfile). */
#define OSM_HINT_STATUS      10                     /* Instance status block */
        /* Every database instance writes out its instance status block
         * about every three seconds. It is read if it is necessary to recover
         * the instance after it fails. */
#define OSM_HINT_ARCHIVE     11                               /* Archive log */
        /* If this is a write then an archive log is being created from an
         * online log. If this is a read then the archive log is being read
         * for recovery to apply its redo. */
#define OSM_HINT_ARCH_BACKUP 12             /* Backup/restore of archive log */
        /* This hint is for copying an archive log to or from tape. If this is
         * a write then it is likely that this restore will soon be followed
         * by a read to apply the log during a recovery. */
#define OSM_HINT_BACKUP      13                           /* RMAN backup set */
        /* This is either a creation of a backup set or reading a backup set
         * for restoring its contents to the database. The backup set may be
         * a full or incremental backup of one or more datafiles. It may also
         * contain multiple archive logs, but this is not likely since there
         * is no point in copying archive logs to a backup set on disk. This
         * hint is also passed when a backup set is copied to or from tape. */
#define OSM_HINT_METADATA    14                              /* OSM metadata */
        /* This is a read or write of the metadata that OSM maintains to find
         * files within a disk group. If the offset into the disk is zero then
         * this is the disk header. Since OSM never caches the disk header,
         * it is advantageous if the storage can cache it. */
#define OSM_HINT_RELOCATE    15                 /* Relocation of data by OSM */
        /* This indicates that the I/O is for the purpose of relocating an
         * extent from one disk to another. The data being transfered is the
         * data that is being relocated. */
#define OSM_HINT_DUMPSET     16              /* dumpsets created by DataPump */
        /* Dumpsets are the equivalent of export files. They are read and
         * written by the DataPump feature in the Oracle kernel. */
#define OSM_HINT_FTP         17           /* FTP into or out of a disk group */
        /* This hint indicates that the data will be sent out an FTP connection
         * or was just read in from an FTP connection. */
#define OSM_HINT_REWIND      18                                /* Rewind log */
        /* This is an I/O to a rewind log. If the rewind feature is enabled,
         * this will be set when writing before images to the rewind log as
         * part of normal database operation. The rewind log is read for
         * unusual recovery situations. */

  osm_handle disk_osm_ioc;                                 /* disk to access */
  /* This is a handle returned by osm_open for the disk to be accessed by this
   * operation. The handle must still be valid. The handle must not be closed
   * by osm_close and the execution thread that opened the disk must still be
   * running. */
  
  ub8     first_osm_ioc;           /* first block, key, or fence of transfer */
  /* This indicates where on the disk the data transfer begins. This is a
   * block number for OSM_READ or OSM_WRITE. It is a disk key number for
   * OSM_SETKEY or OSM_GETKEY. It is a fence key number for OSM_SETFENCE or
   * OSM_GETFENCE. It is a partition number for OSM_GETTAG or OSM_SETTAG. This
   * is zero based so that the first block, key, or partition is number
   * zero. If the beginning of the disk is not available for use by Oracle
   * then the library must protect the first sectors. Oracle will read and
   * write block zero. */

  ub4     rcount_osm_ioc;                                   /* Request count */
  /* This is the number of blocks, tags, keys, or fences to transfer.  It is an
   * error if the first plus rcount is greater than the number of blocks, keys,
   * fences, or partitions. */

  ub2     xor_osm_ioc;                 /* 16 bit XOR of entire data transfer */
  /* If OSM_XOR is set in operation_osm_ioc then this contains the 16 bit wide
   * XOR of the data in each application block. If the write is larger than
   * one block then each application block will XOR to this value. If
   * abs_osm_ioc is equal to rcount_osm_ioc then this is the XOR of all the
   * data in the write. This may be used by the storage device to reject a
   * write of data that has been damaged on its way to the disk. */

  
  ub2     abs_osm_ioc;          /* application block size in physical blocks */
  /* If this is not zero, then it is the block size, in physical blocks, used
   * by the application for the data being transfered.  It is a negative error
   * if rcount_osm_ioc is not an integer multiple of abs_osm_ioc.  If
   * possible, the disk should attempt to avoid partial writes of an
   * application block. All the data for a block should be transfered from the
   * host before making any of it persistent so that a host failure cannot
   * result in a fractured block. However if OSM_OK_FRACTURE is set there is
   * no point in doing this.  The application block size is also used to find
   * application block numbers and to calculate XOR's in multi-block
   * transfers. These verifications cannot be done if the application block
   * size is zero. */

  ub4     abn_offset_osm_ioc;     /* byte offset to application block number */

  ub4     abn_osm_ioc;            /* value of first application block number */

  ub4     abn_mask_osm_ioc;                      /* mask to limit comparison */
  /* These 3 fields describe the block number in an application block on a
   * write. If the OSM_ABNCHK flag is set these fields can be interpreted by
   * the disk to ensure it has received the correct data buffer. The block
   * number starts at some offset in the block. If multiple application blocks
   * are being transfered then the next application block will contain a value
   * that is one greater. The value of the first application block number is
   * in abn_osm_ioc. The offset in bytes to the block number in each
   * application block is in abn_offset_ioc. The application block size in
   * disk blocks is in abs_osm_ioc. The expected values in the data buffer are
   * one greater for each subsequent application block. The value in
   * abn_mask_osm_ioc is anded with the expected block number and the value in
   * the data before making a comparison. This supports striping and small
   * block numbers. If the data received for the write does not match this
   * pattern, then the disk should give an error and not write the data. Note
   * that the storage device needs to know the byte ordering of the host that
   * is sending the data. The validation is different for a host that stores a
   * ub4 with the most significant byte first than for one that stores least
   * significant byte first. This is not in the interface since the library
   * should already know the byte order. */
  
  ub8    tag_osm_ioc;                /* expected tag if this is a disk write */
  /* If the OSM_TAGVALID flag was set for this disk and this is a write
   * command with OSM_TAGCHK set, then the tag for the write is in this
   * field. This tag must match the tag stored in the disk for the partition
   * being written. If the blocks are within more than one overlapping
   * partitions, then the tag only has to match one of the partitions. Any
   * sectors outside of all tagged partitions must not have OSM_TAGCHK set. */

  ub8    reserved_osm_ioc;                                       /* Reserved */
  /* This is an 8 byte field which is never modified by Oracle.  It may be
   * used by osmlib to find its internal data structure associated with this
   * control block.  */

  void   *buffer_osm_ioc;                 /* buffer address for the transfer */
  /* This is the address of the memory buffer for this I/O operation.  The
   * buffer may be in either shared memory or execution thread private
   * memory. It will be aligned to the platform specific alignment required to
   * do DMA directly to the buffer from a storage device. If the opcode is
   * OSM_COPY then this will be null. */

  osm_check *check_osm_ioc;                 /* pointer to check data/results */
  /* If OSM_KEYCHK is set in operation_osm_ioc then this will point to an
   * osm_check structure that describes the check to be done, and receives the
   * actual keys if the check fails. */

  osm_ioc *link_osm_ioc;     /* pointer to destination osm_ioc list for copy */
  /* If the opcode is OSM_COPY then this field points to a list of osm_ioc's
   * to describe the destination disks and offsets. The destinations can also
   * have a disk key and fence key to validate. The destination control block
   * must have an opcode of OSM_WRITE. Its link_osm_ioc field could point to
   * another write osm_ioc. If this field is null then there are no more
   * destinations for the data. */
  
};


/*---------------------------------------------------------------------------*/
/*                         PUBLIC FUNCTIONS                                  */
/*---------------------------------------------------------------------------*/

/*---------------------------- osm_version ----------------------------------*/
uword  osm_version( ub4 *version, osm_iid *iid, oratext *name,  uword len,
                    uword *interface_mask );

/* osmlib API version bits supported */
#define    OSM_API_STUB  0x1                                 /* stub version */
#define    OSM_API_V1    0x2                                /* API version 1 */

/* return values */
#define OSM_INIT_SUCCESS   0                    /* successful initialization */
#define OSM_INIT_VERSION   1                      /* no common version found */
#define OSM_INIT_INSTALL   2                   /* driver/agent not installed */
#define OSM_INIT_PERMIT    3           /* permission to access osmlib denied */
#define OSM_INIT_OTHER     4                    /* some other error occurred */

/*
   NAME:
     osm_version  - Version handshake and instance initialization

   PARAMETERS:
     version(IN/OUT)     -  osmlib API version number
     iid(OUT)            -  Instance identifier
     name(OUT)           -  library description for alert log
     len(IN)             -  length of name buffer
     interface_mask(OUT) -  bit mask of supported interfaces
     
   REQUIRES:

   DESCRIPTION:

     osm_version() is the first call to the osmlib library when an Oracle
     instance is starting. It determines which version of the osmlib library
     is linked with Oracle and if the operating system is correctly configured
     to use the library. By default Oracle ships with an osmlib library that
     only supports the stub protocol. That protocol allows osm_version() to
     return the stub version as the supported version. All other osmlib entry
     points for the stub protocol report an internal error if called. Other
     vendors may supply osmlib libraries that fully support the version one
     protocol defined in this header file.

     The version variable allows Oracle to negotiate with the library to
     decide which version of the osmlib API is to be used. This allows both
     backward and forward compatibility for both the library and Oracle. The
     caller sets the bit for every API version that it can support. If the
     call succeeds then osm_version() will have cleared all but one of the
     version flags. This is the version that will be used. Oracle can decide
     on calling different osmlib interface routines depending on this return
     value. The initial release supports two versions - stub and version 1,
     described in this file.

     If the versioning is successful then an instance id may be returned by
     osm_version(). Oracle does not interpret this value. It is simply passed
     to every osm_init() call done by threads in this Oracle instance. The
     instance id must remain valid even if the thread of execution that called
     osm_version() terminates. It can be invalidated when all threads of
     execution that passed it to osm_init() have terminated or called
     osm_fini(). The instance id will not be used on any other node or after a
     reboot of this node. Disk identifiers will only be shared among execution
     threads that pass the same instance identifier to osm_init(). The library
     may simply ignore the instance ids if it has no use for this
     functionality.

     A name for the library is returned to help administrators know that the
     correct osmlib library has been installed. This string will be printed on
     one line in the alert log no matter what the outcome of the osm_discover()
     call. It must fit in len bytes including a terminating null. If len is
     not long enough then the string must be truncated. The string should
     include the name of the vendor and the release number of the library.
     For the first release len will be 64, but this could change. 

     The interface_mask parameter points to a place to return the capabilities
     of this library. It is a bit mask of the same bits which appear in
     interface_osm_name. It receives the most bits that the library could ever
     set in interface_osm_name for any disk it could discover.

   RETURNS:

     Unlike all other osmlib routines osm_version() does not return an error
     code. It returns one of the constants defined above to indicate the
     success of the versioning. This is necessary because there is no context
     for calling osm_error() to report an error. If the return value is
     anything other than OSM_INIT_SUCCESS then the Oracle instance will fail
     to start. In any case the name of the library and the results of the call
     will appear in the alert log. Note that the stub version of the library
     always is successful.

   NOTE:

     This call may allocate resources to be shared by all execution threads
     in the Oracle instance using the same instance id. However these resources
     cannot be in user space. This call cannot allocate a shared memory
     segment for all execution theads to attach. Resource sharing must be
     done within the operating system so that the appropriate cleanup can
     be performed when an execution thread dies.

*/

/*------------------------------ osm_init -----------------------------------*/
osm_erc  osm_init( osm_iid iid, osm_ctx *ctxp );
/*
   NAME:
     osm_init - Initialize osmlib for this thread of execution

   PARAMETERS:
     iid(IN)       -  Instance id from osm_version()
     ctxp(IN/OUT)  -  osmlib context pointer (cleared before call)

   REQUIRES:
     osm_version() must have successfully returned an instance id

   DESCRIPTION:

     This routine initializes the osmlib library for this thread of execution.
     Its main purpose is to allocate and initialize a context that is passed
     to all osmlib entry points. The pointer to the context structure is
     returned through ctxp.  The context structure is allocated as thread
     private memory. It is normally allocated by a call to malloc(). The
     osmlib library may not declare any global or static variables. This is
     necessary due to the way the Oracle kernel manages its memory.

     This call connects this thread of execution with its instance through the
     instance id.  The instance id must be from an osm_version() call made on
     the same node of the cluster. Note that the execution thread that calls
     osm_version() also calls osm_init() to initialize itself.

     If there is an error then osm_error() will be called to report the error
     and then osm_fini() will be called to clean up the context. Note that this
     means the context pointer must be initialized even if it was impossible
     to create the context. To simplify this problem Oracle will initialize
     the pointer to null before calling osm_init(). The null pointer could be
     used to indicate this situation to osm_error() and osm_fini().

   RETURNS:

     Returns zero if successful, a negative error code if a software bug is
     encountered, or a positive error code if a hardware or user error is
     encountered. The error code is converted to text by calling osm_error().

   NEGATIVE ERROR EXAMPLES:

     An impossible instance id.

     *ctxp not initialized to zero.

   POSITIVE ERROR EXAMPLES:

     Execution thread does not have privilege needed to access osmlib.

     Insufficient memory to allocate context

   NOTE:

     This call may not attach to a shared memory segment for communication
     with other threads executing osmlib. There is no mechanism for cleaning
     up state in a shared memory context if a thread of execution dies while
     holding an osmlib golbal resource. It is presumed that osmlib shared
     state is maintained inside the operating system. The osmlib code must
     continue to function correctly when any thread of execution suddenly
     terminates between any two instruction in osmlib.
*/


/*------------------------------ osm_fini  ---------------------------------*/
osm_erc  osm_fini( osm_ctx ctx );

/*
   NAME:
     osm_fini - Finished calling osmlib for this thread of execution

   PARAMETERS:
     ctx(IN)  -  osmlib context pointer initialized by osm_init()

   REQUIRES:

     osm_init() called to attempt initialization

   DESCRIPTION:

     This routine is called when an execution thread is gracefully cleaning
     itself up to exit. It may not be called when exiting due to an error or
     when the thread of execution is killed by an operating system command. It
     might not be needed for some implementations since clean up at exit may
     be sufficient.

     The primary responsibility of osm_fini() is to release the memory
     allocated for the context and any other memory structures that have not
     yet been freed.

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible. An error is basically
     ignored but the numeric value will be reported in the alert log.

   NEGATIVE ERROR EXAMPLES:

     An invalid context pointer.

*/



/*------------------------------ osm_error ---------------------------------*/
osm_erc  osm_error( osm_ctx ctx, osm_erc errcode, oratext *errbuf, 
                    uword eblen );

/*
   NAME:
     osm_error - Translate an error code into an error message

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     errcode(IN)  -  error code returned by the previous osmlib call
     errbuf(OUT)  -  error string buffer
     eblen(IN)    -  error string buffer length

   REQUIRES:

     osm_init() called to attempt initialization

   DESCRIPTION:

     osm_error() returns a string in errbuf describing the error specified by
     errcode, which is the error code returned by the previous call to the osm
     interface.  Both positive and negative error codes are converted to
     strings by this routine. The size of the buffer is given by eblen, and
     the string returned is truncated to that length, if necessary.  The
     returned string is always null-terminated, and always fits in the
     buffer. It is not an error if the message must be trimmed to fit in the
     buffer. The buffer will be at least 72 bytes long.

     The osm_error() routine is called after a non-zero error code is
     returned, but before any other osmlib routines are called. This ensures
     that the error context, if any, is preserved for osm_error(). Information
     about the last error may be kept in the osmlib context in some
     implementations.  Note that osm_init(), which constructs the osmlib
     context, may itself return an error and not construct the osmlib
     context. In this case the context pointer passed to osm_error() is
     whatever osm_init() stored, or null if it did not store anything.

     All osmlib routines except osm_version() have an error code as their
     return value. osm_fini() may return an error code, but it destroys the
     osm context so its return code is never passed to
     osm_error(). osm_error() itself may return an error code but that is
     never passed to osm_error() to avoid recursive looping. All other
     non-zero return codes will be passed to osm_error(). However the osmlib
     library must not rely on osm_error always being called. For example it
     would be wrong to have osm_error() clean up some state that must be
     cleaned up before other osmlib calls can succeed.

     The string returned must contain only printable characters. In an ASCII
     character set that would be characters in the range 0x20 to 0x7e. The
     string must never contain control characters such as newline or tab.

     Note that I/O control blocks can also contain an error code. However
     those error codes are passed to osm_ioerror() not osm_error().

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible. Any error will be
     reported as an internal Oracle error, but will *NOT* be passed to
     osm_error().

   NEGATIVE ERROR EXAMPLES:

     ctx is not the context created for this thread.

     errcode is not a valid error code.

     errbuf is an invalid pointer.

     eblen is less than 1 (a null terminated string can not be returned).  */


/*------------------------------ osm_discover -------------------------------*/
osm_erc osm_discover( osm_ctx ctx, oratext *setdesc );
/*
   NAME: 
     osm_discover - Discover the names of disks in a disk set

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     setdesc(IN)  -  null terminated discovery string describing the disk set
     
   REQUIRES:

     osm_init() successfully created a context

   DESCRIPTION:

     osm_discover() takes a discovery string from the user and constructs a
     set of disks that are named by the discovery string. The osm_name
     structures for all the disks in the set can then be retrieved via
     osm_fetch(). The format of the string and its interpretation are left to
     the library. Oracle never looks at the string.

     If a fetch from a previous discovery returned a unique disk id string, or
     an access path name string, then those strings must be valid discovery
     strings that will discover exactly the one disk that was previously
     fetched. The access path name will only be used on the same node that
     previously fetched the osm_name, but the unique disk id must be a valid
     discovery string from any node in the cluster.

     Oracle gets discovery strings from initialization parameters, string
     arguments in commands, or from osm_name structures returned by
     osm_fetch(). This allows the syntax of the string to be completely up
     to the implementor of osmlib. The syntax should be documented along
     with the documentation for installing the osmlib software. The syntax
     could be something similar to glob on Unix, or it could be more custom
     to the osmlib implementation. For example the name of a storage array
     could be interpreted as discovering all LUNs on the array that are
     enabled for OSM access.

     If disks are added to or dropped from the disk set immediately after
     osm_discover() returns, they might or might not be returned by
     osm_fetch(). Thus a call to osm_open() is not guaranteed to succeed just
     because its name was fetched. However this should be the only reason for
     osm_open() to fail. If the caller does not have access rights to a disk
     then the disk should not be discovered.

     Discovery is used for two fundamentally different purposes. When creating
     or growing a disk group, discovery is used to identify a set of unused
     disks to put into the same disk group. At startup time discovery is used 
     to find all the disks that are already part of all existing disk groups
     so that the disk groups may be accessed.

     Discovery at startup gets one or more discovery strings from an OSM
     initialization parameter. For each string, osm_discover() is called to
     find all the disks that OSM may be interested in. The header of every
     discovered disk is read to determine if it is an OSM disk and if so to
     which disk group it belongs. It would work if discovery returned all
     disks that are accessible to the host, but it is quicker if the discovery
     string avoids looking at uninteresting disks. Note that startup will fail
     if discovery finds the same disk header under two different names.

     Discovery for adding disks gets a discovery string from the add/create
     command. The string is passed to osm_discover() to find a set of unused
     disks. This is typically a smaller set than found by discovery at
     startup. It is OK if disks already in the same disk group are discovered
     since they will be ignored based on the name matching an existing open
     disk. The rest of the disks will have a header written to them, and
     pieces of existing files will be moved to them. One means of getting the
     discovery string for an add is to use the unique disk id from a previous
     discovery. An administrator can view a list of disks discovered at
     startup and choose an unused one to add to a disk group. The unique disk
     id from the startup discovery is used as the discovery string to identify
     the disk to add.

     Thus the syntax of the discovery string needs to be flexible enough to
     easily discover a large number of disks, as well as capable of specifying
     a very small set of disks. It would be useful to allow specification of
     disks by their qualities as well as by location. It is normal for all the
     disks in a disk group to have similar performance characteristics, so it
     makes sense to allow specification of these characteristics in a discovery
     string used to add disks to a group. It also makes sense for the syntax
     to include concepts the administrator already uses for managing storage.
     For example, each storage array has a unique name so it makes sense to
     allow an array name in a discovery string to limit discovery to disks
     in that storage array. The syntax is completely up to the implementor of
     osmlib.
     
   RETURNS:

     Returns zero if successful, a negative error code if a software bug is
     encountered, or a positive error code if a hardware or user error is
     encountered. The error code is converted to text by calling osm_error().

   EXAMPLES:

   NOTES: */


/*-------------------------------- osm_fetch --------------------------------*/
osm_erc osm_fetch( osm_ctx ctx, osm_name *name );
/*
   NAME: 
     osm_fetch    - Fetch names from the discovered set of disks

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     name(OUT)    -  buffer to hold next discovered disk name

   REQUIRES:

     osm_discover() successfully created a discovery set which has not been
     completely fetched.

   DESCRIPTION:

     After a successful call to osm_discover(), osm_fetch() is called
     repeatedly to get the names of the disks from the disk set. When all the
     disks have been fetched, subsequent calls to osm_fetch() return an
     osm_name of all zero. A blksz_osm_name of zero is never returned except
     when fetching is done.

     If the attributes of a disk can change, then it is possible that the
     results of a fetch are stale. For example, if a disk is actually a
     virtual disk that can grow or shrink, then the fetched size may be
     different from the current size. However, the results of a fetch never
     describe a state that was changed before the call to osm_discover.
     

   RETURNS:

     Returns zero if successful, a negative error code if a software bug is
     encountered, or a positive error code if a hardware or user error is
     encountered. The error code is converted to text by calling osm_error().

   EXCEPTIONS:

   EXAMPLES:

   NOTES: */


/*-------------------------------- osm_open ---------------------------------*/
osm_erc osm_open( osm_ctx ctx, osm_name *name, osm_handle *hand );
/*
   NAME: 
     osm_open     - Open a disk for read/write

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     name(IN)     -  name of disk to open as returned from osm_fetch
     handle(OUT)  -  location to return I/O handle for the disk

   REQUIRES:

     osm_fetch() successfully returned the name of a disk with the OSM_IO
     interface bit set

   DESCRIPTION:

     Open a disk for access through osm_io(). The name of the disk must be
     discovered through osm_discover() and osm_fetch(). The osm_name structure
     returned by osm_fetch() must have OSM_IO set in interface_osm_name.

     The handle returned by this call can be used by any thread of execution
     that constructed its context (osm_ctx) with the same instance id
     (osm_iid) on the same node. The handle becomes invalid if this thread of
     execution terminates, or calls osm_close() on this handle.

   RETURNS:

     Returns zero if successful, a negative error code if a software bug is
     encountered, or a positive error code if a hardware or user error is
     encountered. The error code is converted to text by calling osm_error().

   EXCEPTIONS:

   EXAMPLES:

   NOTES: */

/*-------------------------------- osm_close --------------------------------*/
osm_erc osm_close( osm_ctx ctx, osm_handle handle );
/*
   NAME: 
     osm_close    - Close a disk

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     handle(IN)   -  I/O handle returned by osm_open

   REQUIRES:

     osm_open() successfully returned a handle for this disk

   DESCRIPTION:

     This closes access to a disk through this handle. If there are any I/O's
     in flight when this is called, they may be terminated with an error or
     allowed to complete normally. osm_close must block until any in flight
     I/O's are terminated and any future uses of the handle by any thread of
     execution are prevented. However it must not block for the in flight
     I/O's to be reaped via calls to osm_io. There is no gurantee that reaping
     of a completed I/O happens in a timely manner.
   
   RETURNS:

     Returns zero if successful, a negative error code if a software bug is
     encountered, or a positive error code if a hardware or user error is
     encountered. The error code is converted to text by calling osm_error().

   EXCEPTIONS:

   EXAMPLES:

   NOTES: */

/*------------------------------ osm_io  -----------------------------------*/
osm_erc  osm_io( osm_ctx ctx,
                 osm_ioc *requests[], uword reqlen, 
                 osm_ioc *waitreqs[], uword waitlen, 
                 osm_ioc *completions[], uword complen,
                 ub4 intr, ub4 timeout, uword *statusp );

/* special timeout values */
#define    OSM_NOWAIT    0x0                   /* return as soon as possible */
#define    OSM_WAIT      0xffffffff                         /* never timeout */

/* status flags indicating reasons for return */
#define    OSM_IO_POSTED    0x1                       /* posted to run by OS */
#define    OSM_IO_TIMEOUT   0x2                                   /* timeout */
#define    OSM_IO_WAITED    0x4                        /* wait list complete */
#define    OSM_IO_FULL      0x8                      /* completion list full */
#define    OSM_IO_IDLE      0x10                       /* no more active I/O */

/*
   NAME:
     osm_io - Manage disk I/O requests

   PARAMETERS:
     ctx(IN)         -  osmlib context pointer initialized by osm_init()
     requests(IN)    -  array of osmlib request pointers
     reqlen(IN)      -  number of elements in the request array
     waitreqs(IN)    -  array of osmlib requests to wait for
     waitlen(IN)     -  number of elements in the waitreq array
     completions(OUT)-  array of completed osmlib i/o requests
     complen(IN)     -  completion array length
     intr(IN)        -  i/o wait is interruptible
     timeout(IN)     -  timeout in microseconds
     statusp(OUT)    -  status pointer

   DESCRIPTION:

     This is the I/O interface for the osmlib library.  It is used to submit
     I/O requests, get completed I/O requests, and wait for one or more I/O
     completions. There is an I/O control block for each request of type
     osm_ioc. The control block is used to communicate information about the
     request between Oracle and osmlib. It is allocated and initialized by
     Oracle then passed to osm_io() to start the I/O. osm_io() updates the I/O
     control block and returns a pointer to it when the I/O is
     completed. osm_io() can block until a specific set of I/O control blocks
     are completed. Any errors in the arguments to osm_io() are returned as a
     negative error code.  Errors in the processing of a particular request
     are reported in the control block for that request.

     The requests list is an array of pointers to I/O control blocks that are
     being submitted to start I/O's.  reqlen is a count of the entries in the
     array.  The request list length may be zero, in which case there are no
     control blocks and requests may be invalid (e.g., null).  All the control
     blocks will be used to start new I/O unless errors are encountered in
     processing the arguments of the call to osm_io() or a busy status is set
     due to resource limitations.

     Oracle initializes the status field of the I/O control block to zero. As
     processing proceeds, status bits are set to record progress. If the
     request is successfully queued the OSM_SUBMITTED flag is set. If there
     are insufficient resources to queue the request, then OSM_BUSY will be
     set. Setting OSM_BUSY also sets OSM_FREE and OSM_ERROR because the osm
     library will not return the I/O control block as completed and an error
     code is set to indicate why the request could not be queued. When
     osm_io() returns without an error every control block in the requests
     list will have either OSM_SUBMITTED or OSM_BUSY set. If osm_io()
     encounters a problem with invalid arguments it may return a negative
     error and skip the processing of some of the requests. Skipped requests
     will not have the OSM_SUBMITTED flag set. This is equivalent to never
     having passed the request to the library.

     waitreqs points to a list of I/O control blocks for which the call should
     wait.  waitlen is the count of entries in the array.  If waitlen is zero,
     the waitreqs pointer can be null.  The caller can only wait for requests
     submitted by the same thread of execution with the same osmlib context.
     osm_io() will return when the OSM_FREE flag has been set on every request
     in the wait list. A control block can be in both the requests list and
     the wait list. This accomplishes synchronous I/O. If it is not possible
     to queue the request then its wait is over. If the I/O control block is
     not in the requests list it must have been successfully submitted by a
     prior call to osm_io() and not yet returned. In other words OSM_SUBMITTED
     must be set and OSM_FREE must not be set. osm_io() may consider a request
     as completed if asked to wait for a control block it does not have
     queued. Thus a bad pointer or a pointer to a request submitted by another
     thread of execution may be considered as a completed request which does
     not need to be waited for. This implementation ignores invalid pointers
     and treats them like pointers to control blocks that did not get
     OSM_SUBMITTED set. Alternatively, a pointer to a request that was not
     submitted or made busy, or an invalid pointer, may return a negative
     error from osm_io. This is slightly preferred since it could catch a bug
     that caused a garbage pointer in the waitreqs list.

     osm_io() may return before all the requests in the waitreqs list are
     complete.  This can be the result of reaching the timeout or due to a
     post from the operating system. For example on Unix a signal may be
     received.  The caller must examine the status of the control block for
     the value OSM_FREE to determine whether the I/O has in fact completed.
     An early return from osm_io() does not terminate the I/O request. It may
     be waited for with another osm_io() call or it may be reaped
     asynchronously via the completions list of another osm_io() call.  If the
     status is OSM_FREE when the call returns, then the control block will
     never appear in a completion list. Waiting for a request and having it
     complete is all the notification needed.

     completions points to a zeroed array of pointers to I/O control blocks.
     complen is a count of the entries in the array. When osm_io() returns,
     the completion list is filled with pointers to the I/O control block
     requests, issued by this thread of execution, that have completed.  In
     other words they have OSM_FREE set now and it was not set when osm_io()
     was called. The first null pointer indicates that all subsequent pointers
     are also null - there are no more completions. If complen is zero, no
     completions will be returned, the completions pointer can be null, and
     OSM_IO_FULL will not be set. A zero value for complen is useful for doing
     synchronous I/O or submitting I/O when it is not possible to accept
     completions due to code layering.

     osm_io() will return when the completions list is full. To wait until the
     next 10 I/O's complete, osm_io() is called with a complen of 10. osm_io()
     may return before the completions list is full. This can be the result of
     reaching the timeout, completing requests in the wait list, completing
     all requests, or due to a post from the operating system. osm_io() can 
     also return before completion list is full if it is more efficient to 
     do so. In this case none of the status bits should be set. It is not an
     error for complen to be larger than the total number of requests
     outstanding. In this case osm_io() will return when all I/O has completed
     rather than waiting for the timeout to expire.

     If a waitreqs pointer and a completions pointer are both provided, the
     call waits for the waitreqs requests, then processes any other completed
     requests to the completion list before returning. Thus osm_io() may
     continue to wait after the completion list is full when requests in the
     waitreqs list are not complete. Completions that could not fit in the
     completions list will be returned as completed in a subsequent call to
     osm_io(). If all the requests in the waitreqs list complete then osm_io()
     will return even if there is room left in the completions list.

     A request submitted in an osm_io() can appear in the completion list of
     the same call if it completes in time or has an immediate error such as a
     bad disk address.

     Requests that have status OSM_BUSY set are not returned in the completion
     list.  Requests are returned in the order completed, not the order
     submitted.  If the completion list is not immediately filled, the call
     waits until either the list is filled or the timeout value is exceeded.
     Because the call can return before the completion list is full, completed
     requests must be determined by looking for non-zero pointers in the
     completion list.

     If more completions are requested than there are active I/O requests
     submitted by this execution thread, the call returns when all the
     active I/O requests have been completed.

     Synchronous I/O can be performed by passing a requests list with one or 
     more control blocks, a null completions list, a waitreqs pointing to the 
     requested control blocks, and a timeout value of OSM_WAIT.

     The timeout value, in microseconds, determines how long the call will
     wait for I/O to complete before returning.  It is also possible to return
     before the timeout time due to a post from the operating system. A
     timeout value of OSM_NOWAIT does not wait at all. I/O control blocks in
     the waitreqs list will have their OSM_FREE status set if they have
     already completed. The completions list will only contain requests that
     are already completed when osm_io() finishes processing the requests
     list. A timeout value of OSM_WAIT will never timeout. osm_io() will have
     to return for some other reason. An infinite wait will always return when
     all I/O's have completed so it is not really infinite. A timeout is
     usually used in conjunction with a completions list to gather a group of
     completions for efficiency, but to avoid sleeping too long before
     processing I/O's that are complete. If a timeout value is specified, but
     there is no completion list and no waitreqs list, the timeout has no
     meaning for processing I/O requests, and the timeout value will be
     ignored.  The call will return without setting OSM_IO_TIMEOUT.

     The operating system can post an execution thread that is blocked in
     osm_io() and have the call return immediately. If there is a waitreqs
     list, any completed requests will have OSM_FREE set. If there is a
     completions list, it may have pointers to completed requests. If there is
     a requests list all the requests will be marked as either submitted or
     busy when osm_io() returns. A Unix signal is an operating system
     post. Similar mechanisms exist on other operating systems. Another Oracle
     thread can post an execution thread through the operating system while it
     is in osm_io() to wake it up to receive a message.

     The intr parameter specifies whether the osm_io() call can be posted by
     the operating system. When it is set to TRUE it means that this I/O wait
     must be interruptible. Another Oracle execution thread must be able to
     cause a routine in Oracle to run even if this thread is blocked. For
     example, on Unix this would mean that a signal handler could run while
     waiting for I/O to complete. If the asynchronously called Oracle routine
     needs to have osm_io() return, then it will call osm_posted() to notify
     the library that it was woken up in order to force a return from
     osm_io(). For example, on Unix the signal handler will call osm_posted()
     and return. If intr is set to FALSE, osmlib library may wait for I/O in
     such a manner that it would be impossible to post the execution thread
     out of the I/O wait. However it is acceptable for osm_io() to always
     return when interrupted.
     
     statusp is a pointer to a variable that is is set to indicate why osm_io()
     returned. Since it is possible to have multiple completion conditions at
     the same time, the values are bit masks which can be ORed together to
     show all the reasons. It is not required to show all reasons since race
     conditions can make that impossible. For example the timeout may expire
     after all I/O's in the wait list have completed but before the execution
     thread actually executes. It would be reasonable to only set
     OSM_IO_WAITED in this case. If a call has no completions list and no
     waitreq list, but a timeout is specified, the call will return without
     waiting, and with none of the status flags set. The flags are interpreted
     as follows:

     OSM_IO_POSTED - The operating system woke up the execution thread due to
     some external event, not the completion of the I/O. For example on a Unix
     system this would be set if a signal was caught.

     OSM_IO_TIMEOUT - The timeout expired. This will always be set when the
     timeout value is OSM_NOWAIT and never set when the timeout value is
     OSM_WAIT. The timeout value will have no effect if the waitreqs and 
     completions array are passed in as NULL.

     OSM_IO_WAITED - All the I/O control blocks in the waitreqs list have the
     OSM_FREE status bit set. This cannot be set if waitlen is zero.

     OSM_IO_FULL - The completions list is full. There may be more completed
     requests, but this does not imply that.

     OSM_IO_IDLE - There are no more I/O's to complete so there is nothing to
     wait for. This is not necessarily set when there are no more I/O's and
     there is another reason for returning, such as OSM_IO_WAITED.

   WRITE PERSISTENCE:

     As one would expect, a database depends very heavily on disk writes
     working correctly. It is essential that once osm_io() returns a disk
     write request as completed, that data must be persistent. The next read
     of the data, from any node in the cluster, must return the latest data or
     an error despite any failure in hardware or software. It is extremely
     important that previous versions of the data cannot be returned. This
     means that the write cannot be completed when the data is copied to a
     write back cache which could be lost. If the data is in a non-volatile
     cache then the write can be acknowledged, but there must be a mechanism
     to ensure the disk will be marked invalid if the cache is lost to a
     double failure or a human error.

     The setting of disk keys and fence keys does not have to be persistent.
     If they are lost they may be reinitialized to zero, and OSM will reload
     them. This must not happen very often since reloading requires significant
     amounts of I/O and prevents access to the disk until the reload is
     complete. Common events like a bus reset must not clear keys. Keys must
     be zero when a disk is formatted with keys.

   EXECUTION THREAD TERMINATION

     It is possible that a thread of execution may terminate while it has
     outstanding I/O's. The termination could be either a normal termination
     or abnormal. All I/O's must be terminated before the operating system
     can report the thread as dead. Writes may be canceled or completed as
     long as the correct data is transfered and made persistent before the
     thread of execution is reported dead. A disk read must not modify the
     memory buffer if the thread of execution is reported as dead.

     A common implementation of this requirement is to have a process become
     a zombie until all the outstanding disk I/O's are completed. This does
     prevent memory corruption, but it has a serious drawback. If the disk
     considers the I/O complete, but the notification has been lost, then
     the I/O may never complete. This results in a zombie that will not go
     away. The only way to ensure the zombie does not awaken and do a write
     is to reboot the system. This is quite disruptive and should be avoided.

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible. Any error will be
     reported as an internal Oracle error. When an error is returned, there
     may be some I/O control block requests that were started while others
     were not, due to the error. Requests that were not seen will still have a
     status of zero. They should be treated as if they were never passed to
     osm_io(). Requests with OSM_SUBMITTED or OSM_BUSY set have been passed to
     osm_io() and must be reaped as appropriate.

     Positive errors are not possible in the return code since it is an error
     that applies to the call itself and not to any particular I/O request.
     The only call errors are impossible values in the arguments which imply
     software bugs.

     Errors for a particular I/O request are recorded in the I/O control
     block. Those errors can be either positive or negative. Some negative
     errors are not detected until after osm_io() starts some of the I/O
     requests or returns, and thus are not errors on the call. Errors in an
     I/O control block are indicated by setting the OSM_ERROR flag when
     OSM_FREE is set.  When this happens, the I/O control block is usually
     passed to osm_ioerror() to get a descriptive error message. The 
     information stored in the I/O control block should be sufficient to 
     generate the error message at a later point in time. The implementor is 
     free to use the elaptime_osm_ioc and reserved_osm_ioc fields to store any
     additional information regarding the i/o error. The other fields, such as
     operation_osm_ioc and first_osm_ioc, will be the same as when the
     operation completed.

   NEGATIVE CALL ERROR EXAMPLES:

     ctx is not the context created for this thread.

     requests is an invalid address.

     requests contains an I/O control block pointer to an invalid address.

     completions is an invalid address.

     waitreqs is an invalid address.

     statusp is an invalid address.

     waitreqs contains a pointer that does not point to an active request or a
     request in the requests list.

     completions contains a list entry that is non-zero.

     status_osm_ioc of one of the i/o control blocks in requests is not 
     set to zero.

   NEGATIVE REQUEST ERROR EXAMPLES:

     disk_osm_ioc is not a valid disk handle returned by a successful
     osm_open() executed by this instance.

     buffer_osm_ioc is an invalid pointer.

     operation_osm_ioc is not a valid operation.
                         
     priority_osm_ioc is not valid value.

   POSITIVE REQUEST ERROR EXAMPLES:

     The hardware reported a failure when accessing the media.

     Only part of the data could be transferred.

   NOTE:

     It is normal for a SCSI driver to reissue a command if a bus reset puts
     the outcome of the command in doubt. It may not be possible to determine
     if the disk received or processed the command. A simple disk read or
     write can be repeated and the results are the same - it is idempotent.
     However an I/O that validates a disk key could succeed the first time,
     but fail when repeated due to a setkey operation changing the disk key
     value before the repeat. For OSM requests a failure after success is
     tolerated. This interface allows the library to issue a request multiple
     times if the result is indeterminate. A key mismatch on write does not
     mean that the disk was not modified since the mismatch may have happened
     on a retry.

*/

/*------------------------------ osm_ioerror --------------------------------*/
osm_erc  osm_ioerror( osm_ctx ctx, osm_ioc *ioc, oratext *errbuf, 
                      uword eblen );

/*
   NAME:
     osm_ioerror - Translate an I/O error code into an error message

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     ioc(IN)      -  I/O control block with OSM_ERROR and OSM_FREE set
     errbuf(OUT)  -  error string buffer
     eblen(IN)    -  error string buffer length

   REQUIRES:

     osm_io() returned control block with error recorded in it.

   DESCRIPTION:

     osm_ioerror() returns a string in errbuf describing the error recorded in
     an I/O control block. Both positive and negative error codes are
     converted to strings by this routine. The size of the buffer is given by
     eblen, and the string returned is truncated to that length, if necessary.
     The returned string is always null-terminated, and always fits in the
     buffer. It is not an error if the message must be trimmed to fit in the
     buffer. The buffer will be at least 72 bytes long.

     This is called for every osm_ioc that is made free with OSM_ERROR set in
     the status field.  However the osmlib library must not rely on
     osm_ioerror() always being called.

     The string returned must contain only printable characters. In an ASCII
     character set that would be characters in the range 0x20 to 0x7e. The
     string must never contain control characters such as newline or tab.

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible. Any error will be
     reported as an internal Oracle error.

   NEGATIVE ERROR EXAMPLES:

     ctx is not the context created for this thread.

     ioc is an invalid pointer.

     ioc does not point to an I/O control block with both OSM_FREE and
     OSM_ERROR set

     errbuf is an invalid pointer.

     eblen is less than 1 (a null terminated string can not be returned).

*/

/*------------------------------- osm_iowarn --------------------------------*/
osm_erc  osm_iowarn( osm_ctx ctx, osm_ioc *ioc, oratext *wrnbuf, 
                      uword wblen );

/*
   NAME:
     osm_iowarn - Translate an I/O warning code into a message

   PARAMETERS:
     ctx(IN)      -  osmlib context pointer initialized by osm_init()
     ioc(IN)      -  I/O control block with OSM_WARN and OSM_FREE set
     wrnbuf(OUT)  -  warning string buffer
     wblen(IN)    -  warning string buffer length

   REQUIRES:

     osm_io() returned control block with warning recorded in it.

   DESCRIPTION:

     osm_iowarn() returns a string in wrnbuf describing the warning recorded
     in an I/O control block. The size of the buffer is given by wblen, and
     the string returned is truncated to that length, if necessary.  The
     returned string is always null-terminated, and always fits in the
     buffer. It is not an error if the message must be trimmed to fit in the
     buffer. The buffer will be at least 72 bytes long.

     This is not called for every osm_ioc that is made free with OSM_WARN set
     in the status field. Repeated warnings with the same code will not flood
     the warning mechanism with the same message.

     The string returned must contain only printable characters. In an ASCII
     character set that would be characters in the range 0x20 to 0x7e. The
     string must never contain control characters such as newline or tab.

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible. Any error will be
     reported as an internal Oracle error.

   NEGATIVE ERROR EXAMPLES:

     ctx is not the context created for this thread.

     ioc is an invalid pointer.

     ioc does not point to an I/O control block with both OSM_FREE and
     OSM_WARN set

     wrnbuf is an invalid pointer.

     wblen is less than 1 (a null terminated string can not be returned).

*/

/*----------------------------- osm_cancel  ---------------------------------*/
osm_erc  osm_cancel( osm_ctx ctx, osm_ioc *ioc );
/*
   NAME:
     osm_cancel - Cancel an I/O request that is being processed.

   PARAMETERS:
     ctx(IN)  -  osmlib context pointer returned by osm_init()
     ioc(IN)  -  osmlib I/O control block to be cancelled

   REQUIRES:

     osm_io() successfully submitted the I/O control block

   DESCRIPTION:

     osm_cancel() cancels the I/O request for the control block pointed to by
     ioc.  OSM_CANCELLED will be set in status_osm_ioc.  If possible the I/O
     will be terminated early, otherwise it may complete normally.  In either
     case a normal osm_io() completion call must be issued to set the OSM_FREE
     status flag. No error is set in the control block just because of a
     cancel, but there may be an error already recorded. A cancelled request
     may be partially complete. Cancelling a write to a mirrored disk may
     leave the disk sides out of sync.

     The osm_cancel() call never blocks waiting for I/O to be completed.  A
     request can only be cancelled by the same execution thread that started
     the I/O.  It is an error for ioc to point to a request with status flag
     OSM_FREE set.

     This call can be implemented as nothing more than setting the
     OSM_CANCELLED status bit - always allowing the I/O to complete. This
     could make clean up after some types of failure take longer than desired.

   RETURNS:

     Returns zero if successful or a negative error code if a software bug is
     encountered. A positive error code is not possible.

   NEGATIVE ERROR EXAMPLES:

     ctx is not the context created for this thread.

     ioc is an invalid pointer.

     ioc does not point to an active I/O control block 


*/


/*----------------------------- osm_posted  ---------------------------------*/
void  osm_posted( osm_ctx ctx );
/*
   NAME:
     osm_posted - Thread is posted out of its I/O wait

   PARAMETERS:
     ctx(IN  )     -  osmlib context pointer returned by osm_init()

   REQUIRES:

     osm_io() called to wait for I/O completion in an interruptible way

   DESCRIPTION:
    
     When a thread waiting for I/O completions gets posted by the operating 
     system, osm_posted() is called in the waiting threads context. This
     tells osm_io() that it should return immediately rather than continue to
     wait for I/O completions. This will only be called if the intr flag to
     osm_io() was true.

     Note that there are some race conditions possible. The asynchronous call
     may happen before osm_io() gets around to waiting or after it wakes up.
     It is acceptable for such a post to occasionally get lost, but there may
     be performance problems if it happens very often.

     This mechanism is most useful on operating systems where there is no
     indication of why an execution thread has woken up. This provides
     notification that the execution thread was woken up to end its wait on
     I/O.
     
     
   RETURNS:

     Nothing

   EXAMPLES:
   
     On Unix Oracle uses a signal to post a process that is waiting on I/O.
     The signal handler will call osm_posted() and return. The library could
     implement osm_posted() to simply set a flag in the osmlib context. When
     osm_io() is called it would first clear the flag. If it receives an EINTR
     error when it waits for I/O, it would check to see if the flag is set. If
     it is set then osm_io() returns, otherwise it waits again for I/O
     completion. It would also be acceptable to return on any EINTR return
     code.

     On NT Oracle makes an asynchronous procedure call to wake up an execution
     thread that is blocked on I/O. If called the procedure calls osm_posted()
     and returns.  The library could implement osm_posted() to set a flag in
     the osmlib context. When osm_io() is called it would first clear the
     flag. When called with intr TRUE, osm_io() would wait for I/O
     interruptibly. If the flag is set when it wakes then osm_io() returns
     without waiting for any more I/O completions.

*/


#endif                                                      /* OSMLIB_ORACLE */
