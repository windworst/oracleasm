/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmdisk.h - Oracle Automatic Storage Management disk defines.
    
  DESCRIPTION
  
  This file is an internal header to the asmlib implementation on Linux.
*/


#ifndef _ASMDISK_H
#define _ASMDISK_H

/*
 * Defines
 */

/*
 * Disk label.  This is a 32 byte quantity at offset 32 (0x20) on the 
 * disk.  The first 8 bytes are "ORCLDISK".  The remaining 24 bytes
 * are reserved for future use.
 */
#define ASM_DISK_LABEL_MARKED   "ORCLDISK"
#define ASM_DISK_LABEL_CLEAR    "ORCLCLRD"
#define ASM_DISK_LABEL_OFFSET   32

struct asm_disk_label {
	char dl_tag[8];
	char dl_id[24];
};

#endif  /* _ASMDISK_H */
