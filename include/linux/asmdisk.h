/*
 * NAME
 *	asmdisk.h - ASM library disk tag.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This file contains the definition of the ASM library's disk
 *      tag.  This tag allows recognition of ASM disks.
 *
 * MODIFIED   (YYYY/MM/DD)
 *      2004/01/02 - Joel Becker <joel.becker@oracle.com>
 *              Initial LGPL header.
 *
 * Copyright (c) 2002-2004 Oracle Corporation.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License, version 2 as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have recieved a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

/*
 * This file is an internal header to the asmlib implementation on
 * Linux.
 */


#ifndef _ASMDISK_H
#define _ASMDISK_H

/*
 * Defines
 */

/*
 * Disk label.  This is a 32 byte quantity at offset 32 (0x20) on the 
 * disk.  The first 8 bytes are "ORCLDISK".  The remaining 24 bytes
 * are a unique device label determined by the administrator.
 */
#define ASM_DISK_LABEL_MARKED   "ORCLDISK"
#define ASM_DISK_LABEL_CLEAR    "ORCLCLRD"
#define ASM_DISK_LABEL_OFFSET   32

struct asm_disk_label {
	char dl_tag[8];
	char dl_id[24];
};

#endif  /* _ASMDISK_H */
