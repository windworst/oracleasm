/*
 * NAME
 *	asmmanager.h - ASM management device.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This file contains routines for managing the ASM kernel manager
 *      device.  The library communicates to the kernel driver through
 *      this device.
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


#ifndef _ASMMANAGER_H
#define _ASMMANAGER_H

/*
 * Defines
 */

/*
 * Path-fu for the ASM manager device.  This is where a particular
 * oracleasmfs is mounted.  Default is ASM_MANAGER_DEFAULT
 */
#define ASM_MANAGER_DEFAULT		"/dev/oracleasm"
#define ASM_MANAGER_DISKS		"disks"
#define ASM_MANAGER_INSTANCES		"iid"

#ifndef __KERNEL__
static inline char *asm_disk_path(const char *manager, const char *disk)
{
	size_t len;
	char *asm_disk;

	if (!manager || !*manager || !disk)
		return NULL;

	len = strlen(manager) + strlen("/") +
		strlen(ASM_MANAGER_DISKS) + strlen("/") + strlen(disk);
	asm_disk = (char *)malloc(sizeof(char) * (len + 1));
	if (!asm_disk)
		return NULL;
	snprintf(asm_disk, len + 1, "%s/%s/%s", manager,
		 ASM_MANAGER_DISKS, disk);

	return asm_disk;
}


static inline char *asm_manage_path(const char *manager)
{
	size_t len;
	char *asm_manage;

	if (!manager || !*manager)
		return NULL;
	len = strlen(manager) + strlen("/") +
		strlen(ASM_MANAGER_INSTANCES);
	asm_manage = (char *)malloc(sizeof(char) * (len + 1));
	if (!asm_manage)
		return NULL;
	snprintf(asm_manage, len + 1, "%s/%s", manager,
		 ASM_MANAGER_INSTANCES);

	return asm_manage;
}


#define ASM_MANAGER_IID_FORMAT		"%.8lX%.8lX"
static inline char *asm_iid_path(const char *manager,
				 unsigned long iid_high,
				 unsigned long iid_low)
{
	size_t len;
	char *asm_iid;

	if (!manager || !*manager)
		return NULL;
	len = strlen(manager) + strlen("/") +
		strlen(ASM_MANAGER_INSTANCES) + strlen("/") +
		(8 + 8 + 1);  /* 8 chars per u32, 1 char for '.' */
	asm_iid = (char *)malloc(sizeof(char) * (len + 1));
	if (!asm_iid)
		return NULL;
	snprintf(asm_iid, len + 1, "%s/%s/" ASM_MANAGER_IID_FORMAT,
		 manager, ASM_MANAGER_INSTANCES, iid_high, iid_low);

	return asm_iid;
}
#endif  /* __KERNEL__ */
#endif  /* _ASMMANAGER_H */
