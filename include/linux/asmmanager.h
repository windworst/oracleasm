/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmmanager.h - Oracle Automatic Storage Management manager defines.
    
  DESCRIPTION
  
  This file is an internal header to the asmlib implementation on Linux.
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

	if (!manager || !*manager || !disk || !*disk)
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
