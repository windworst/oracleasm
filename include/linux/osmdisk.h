/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmdisk.h - Oracle Storage Manager disk defines.
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.
*/


#ifndef _OSMDISK_H
#define _OSMDISK_H

/*
 * Defines
 */

/*
 * Disk label.  This is a 32 byte quantity at offset 32 (0x20) on the 
 * disk.  The first 8 bytes are "ORCLDISK".  The remaining 24 bytes
 * are reserved for future use.
 */
#define ASM_DISK_LABEL          "ORCLDISK"
#define ASM_DISK_LABEL_CLEAR    "ORCLCLRD"
#define ASM_DISK_LABEL_SIZE     8
#define ASM_DISK_LABEL_OFFSET   32

#endif  /* _OSMDISK_H */
