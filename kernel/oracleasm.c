/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * NAME
 *	oracleasm.c - ASM library kernel driver.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This file contains the kernel driver of the Oracle Automatic
 *      Storage Managment userspace library.  It provides the routines
 *      required to support the userspace library.
 *
 * MODIFIED   (YYYY/MM/DD)
 *      2004/01/02 - Joel Becker <joel.becker@oracle.com>
 *		Initial GPL header.
 *      2004/09/10 - Joel Becker <joel.becker@oracle.com>
 *      	First port to 2.6.
 *      2004/12/16 - Joel Becker <joel.becker@oracle.com>
 *      	Change from ioctl to transaction files.
 *
 * Copyright (c) 2002-2004 Oracle Corporation.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2 as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have recieved a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

/*
 * This driver's filesystem code is based on the ramfs filesystem.
 * Copyright information for the original source appears below.
 */

/* Simple VFS hooks based on: */
/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *		 2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */


#include <linux/fs.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/mount.h>
#include <linux/smp_lock.h>
#include <linux/parser.h>
#include <linux/backing-dev.h>

#include <asm/uaccess.h>
#include <linux/spinlock.h>

#include "linux/asmcompat32.h"
#include "linux/asmkernel.h"
#include "linux/asmabi.h"
#include "linux/asmdisk.h"
#include "linux/asmmanager.h"
#include "asmerror.h"

#include "linux/asm_module_version.h"

#if 0
#include "transaction_file.h"
#else
#include "transaction_file.c"  /* XXX ugly for now */
#endif 

#if PAGE_CACHE_SIZE % 1024
#error Oh no, PAGE_CACHE_SIZE is not divisible by 1k! I cannot cope.
#endif  /* PAGE_CACHE_SIZE % 1024 */


/* some random number */
#define ASMFS_MAGIC	0x958459f6


/*
 * Compat32
 */
#define ASM_BPL_32		32
#if BITS_PER_LONG == 32
# define asm_submit_io_32	asm_submit_io_native
# define asm_maybe_wait_io_32	asm_maybe_wait_io_native
# define asm_complete_ios_32	asm_complete_ios_native
#else
# if BITS_PER_LONG == 64
#  define ASM_BPL_64		64
#  define asm_submit_io_32	asm_submit_io_thunk
#  define asm_submit_io_64	asm_submit_io_native
#  define asm_maybe_wait_io_32	asm_maybe_wait_io_thunk
#  define asm_maybe_wait_io_64	asm_maybe_wait_io_native
#  define asm_complete_ios_32	asm_complete_ios_thunk
#  define asm_complete_ios_64	asm_complete_ios_native
# endif  /* BITS_PER_LONG == 64 */
#endif  /* BITS_PER_LONG == 32 */

/* Turn debugging on */
#undef DEBUG 
#undef DEBUG1
#undef DEBUG_BROKEN
#define dprintk(x...) do { ; } while (0)
#define dprintk1(x...) do { ; } while (0)
#undef LOG_ENABLE


#ifndef LOG_ENABLE
#define LOG_ENTRY() do { ; } while (0)
#define LOG_EXIT() do { ; } while (0)
#define LOG_EXIT_RET(x...) do { ; } while (0)
#define LOG(x...) do { ; } while (0)
#else
#define LOG_ENTRY() do { printk("ASM: Entered %s\n", __FUNCTION__); } while (0)
#define LOG_EXIT() do { printk("ASM: Exited %s\n", __FUNCTION__); } while (0)
#define LOG_EXIT_RET(ret) do { printk("ASM: Exited %s with code 0x%lX\n", __FUNCTION__, (unsigned long)(ret)); } while (0)
#define LOG printk
#endif  /* LOG_ENABLE */


static struct super_operations asmfs_ops;
static struct file_operations asmfs_dir_operations;
static struct file_operations asmfs_file_operations;
static struct inode_operations asmfs_file_inode_operations;
static struct inode_operations asmfs_disk_dir_inode_operations;
static struct inode_operations asmfs_iid_dir_inode_operations;

static kmem_cache_t	*asm_request_cachep;
static kmem_cache_t	*asmfs_inode_cachep;
static kmem_cache_t	*asmdisk_cachep;

/*
 * asmfs super-block data in memory
 */
struct asmfs_sb_info {
	struct super_block *asmfs_super;
	/* Prevent races accessing the used block
	 * counts. Conceptually, this could probably be a semaphore,
	 * but the only thing we do while holding the lock is
	 * arithmetic, so there's no point */
	spinlock_t asmfs_lock;

	/* It is important that at least the free counts below be
	   signed.  free_XXX may become negative if a limit is changed
	   downwards (by a remount) below the current usage. */	  

	/* max number of inodes - controls # of instances */
	long max_inodes;
	/* free_inodes = max_inodes - total number of inodes currently in use */
	long free_inodes;

	unsigned long next_iid;
};

#define ASMFS_SB(sb) ((struct asmfs_sb_info *)((sb)->s_fs_info))


struct asmfs_file_info {
	struct file *f_file;
	spinlock_t f_lock;		/* Lock on the structure */
	wait_queue_head_t f_wait;	/* Folks waiting on I/O */
	struct list_head f_ctx;		/* Hook into the i_threads list */
	struct list_head f_ios;		/* Outstanding I/Os for this thread */
	struct list_head f_complete;	/* Completed I/Os for this thread */
	struct list_head f_disks;	/* List of disks opened */
	struct bio *f_bio_free;		/* bios to free */
};

#define ASMFS_FILE(_f) ((struct asmfs_file_info *)((_f)->private_data))


/*
 * asmfs inode data in memory
 *
 * Note that 'thread' here can mean 'process' too :-)
 */
struct asmfs_inode_info {
	spinlock_t i_lock;		/* lock on the asmfs_inode_info structure */
	struct list_head i_disks;	/* List of disk handles */
	struct list_head i_threads;	/* list of context structures for each calling thread */
	struct inode vfs_inode;
};

static inline struct asmfs_inode_info *ASMFS_I(struct inode *inode)
{
	return container_of(inode, struct asmfs_inode_info, vfs_inode);
}

static inline struct inode *ASMFS_F2I(struct file *file)
{
	return file->f_dentry->d_inode;
}

/*
 * asm disk info
 */
struct asm_disk_info {
	struct asmfs_inode_info *d_inode;
	struct block_device *d_bdev;	/* Block device we I/O to */
	int d_max_sectors;		/* Maximum sectors per I/O */
	int d_live;			/* Is the disk alive? */
	atomic_t d_ios;			/* Count of in-flight I/Os */
	struct list_head d_open;	/* List of assocated asm_disk_heads */
	struct inode vfs_inode;
};

static inline struct asm_disk_info *ASMDISK_I(struct inode *inode)
{
	return container_of(inode, struct asm_disk_info, vfs_inode);
}


/*
 * asm disk info lists
 *
 * Each file_info struct has a list of disks it has opened.  As this
 * is an M->N mapping, an intermediary structure is needed
 */
struct asm_disk_head {
	struct asm_disk_info *h_disk;	/* Pointer to associated disk */
	struct asmfs_file_info *h_file;	/* Pointer to owning file */
	struct list_head h_flist;	/* Hook into file's list */
	struct list_head h_dlist;	/* Hook into disk's list */
};


/* ASM I/O requests */
struct asm_request {
	struct list_head r_list;
	struct asmfs_file_info *r_file;
	struct asm_disk_info *r_disk;
	asm_ioc *r_ioc;				/* User asm_ioc */
	u16 r_status;				/* status_asm_ioc */
	int r_error;
	unsigned long r_elapsed;		/* Start time while in-flight, elapsted time once complete */
	struct bio *r_bio;			/* The I/O */
	atomic_t r_bio_count;			/* Atomic count */
};


/*
 * Transaction file contexts.
 */
static ssize_t asmfs_svc_query_version(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_get_iid(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_check_iid(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_query_disk(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_open_disk(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_close_disk(struct file *file, char *buf, size_t size);
static ssize_t asmfs_svc_io32(struct file *file, char *buf, size_t size);
#if BITS_PER_LONG == 64
static ssize_t asmfs_svc_io64(struct file *file, char *buf, size_t size);
#endif

static struct transaction_context trans_contexts[] = {
	[ASMOP_QUERY_VERSION]		= {asmfs_svc_query_version},
	[ASMOP_GET_IID]			= {asmfs_svc_get_iid},
	[ASMOP_CHECK_IID]		= {asmfs_svc_check_iid},
	[ASMOP_QUERY_DISK]		= {asmfs_svc_query_disk},
	[ASMOP_OPEN_DISK]		= {asmfs_svc_open_disk},
	[ASMOP_CLOSE_DISK]		= {asmfs_svc_close_disk},
	[ASMOP_IO32]			= {asmfs_svc_io32},
#if BITS_PER_LONG == 64
	[ASMOP_IO64]			= {asmfs_svc_io64},
#endif
};

static struct backing_dev_info memory_backing_dev_info = {
	.ra_pages	= 0,	/* No readahead */
	.memory_backed	= 1,	/* Does not contribute to dirty memory */
};


static struct inode *asmdisk_alloc_inode(struct super_block *sb)
{
	struct asm_disk_info *d = kmem_cache_alloc(asmdisk_cachep, SLAB_KERNEL);
	if (!d)
		return NULL;
	return &d->vfs_inode;
}

static void asmdisk_destroy_inode(struct inode *inode)
{
	struct asm_disk_info *d = ASMDISK_I(inode);

	if (atomic_read(&d->d_ios))
		BUG();

	if (!list_empty(&d->d_open))
		BUG();

	kmem_cache_free(asmdisk_cachep, d);
}

static void init_asmdisk_once(void *foo, kmem_cache_t *cachep, unsigned long flags)
{
	struct asm_disk_info *d = (struct asm_disk_info *)foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
	{
		memset(d, 0, sizeof(*d));
		INIT_LIST_HEAD(&d->d_open);
		inode_init_once(&d->vfs_inode);
	}
}

static void asmdisk_clear_inode(struct inode *inode)
{
	struct asm_disk_info *d = ASMDISK_I(inode);

	LOG_ENTRY();
	if (atomic_read(&d->d_ios))
		BUG();

	if (!list_empty(&d->d_open))
		BUG();

	if (d->d_live)
		BUG();

	if (d->d_bdev) {
		LOG("asmdisk_clear_inode releasing disk\n");
		bd_release(d->d_bdev);
		blkdev_put(d->d_bdev);
		d->d_bdev = NULL;
	}
	LOG_EXIT();
}


static struct super_operations asmdisk_sops = {
	.statfs			= simple_statfs,
	.alloc_inode		= asmdisk_alloc_inode,
	.destroy_inode		= asmdisk_destroy_inode,
	.drop_inode		= generic_delete_inode,
	.clear_inode		= asmdisk_clear_inode,
};

static struct super_block *asmdisk_get_sb(struct file_system_type *fs_type,
					  int flags,
					  const char *dev_name,
					  void *data)
{
	return get_sb_pseudo(fs_type, "asmdisk:",
			     &asmdisk_sops, 0x61736D64);
}

static struct file_system_type asmdisk_type = {
	.name		= "asmdisk",
	.get_sb		= asmdisk_get_sb,
	.kill_sb	= kill_anon_super,
};

static struct vfsmount *asmdisk_mnt;

static int __init init_asmdiskcache(void)
{
	int err;
	asmdisk_cachep = kmem_cache_create("asmdisk_cache",
					   sizeof(struct asm_disk_info),
					   0, SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT,
					   init_asmdisk_once, NULL);
	if (!asmdisk_cachep)
		return -ENOMEM;
	err = register_filesystem(&asmdisk_type);
	if (err) {
		kmem_cache_destroy(asmdisk_cachep);
		return err;
	}
	asmdisk_mnt = kern_mount(&asmdisk_type);
	if (IS_ERR(asmdisk_mnt)) {
		err = PTR_ERR(asmdisk_mnt);
		unregister_filesystem(&asmdisk_type);
		kmem_cache_destroy(asmdisk_cachep);
		return err;
	}

	return 0;
}

static void destroy_asmdiskcache(void)
{
	unregister_filesystem(&asmdisk_type);
	mntput(asmdisk_mnt);
	kmem_cache_destroy(asmdisk_cachep);
}


static int asmdisk_test(struct inode *inode, void *data)
{
	return ASMDISK_I(inode)->d_inode == (struct asmfs_inode_info *)data;
}

static int asmdisk_set(struct inode *inode, void *data)
{
	ASMDISK_I(inode)->d_inode = (struct asmfs_inode_info *)data;
	return 0;
}



/*
 * Resource limit helper functions
 */


/* Decrements the free inode count and returns true, or returns false
 * if there are no free inodes */
static struct inode *asmfs_alloc_inode(struct super_block *sb)
{
	struct asmfs_sb_info *asb = ASMFS_SB(sb);
	struct asmfs_inode_info *aii;

	aii = (struct asmfs_inode_info *)kmem_cache_alloc(asmfs_inode_cachep, SLAB_KERNEL);

	if (!aii)
		return NULL;

	spin_lock_irq(&asb->asmfs_lock);
	if (!asb->max_inodes || asb->free_inodes > 0) {
		asb->free_inodes--;
		spin_unlock_irq(&asb->asmfs_lock);
	} else {
		spin_unlock_irq(&asb->asmfs_lock);
		kmem_cache_free(asmfs_inode_cachep, aii);
		return NULL;
	}

	return &aii->vfs_inode;
}

/* Increments the free inode count */
static void asmfs_destroy_inode(struct inode *inode)
{
	spin_lock_irq(&ASMFS_SB(inode->i_sb)->asmfs_lock);
	ASMFS_SB(inode->i_sb)->free_inodes++;
	spin_unlock_irq(&ASMFS_SB(inode->i_sb)->asmfs_lock);

	kmem_cache_free(asmfs_inode_cachep, ASMFS_I(inode));
}

static void init_once(void *foo, kmem_cache_t *cachep, unsigned long flags)
{
	struct asmfs_inode_info *aii = (struct asmfs_inode_info *)foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {
		INIT_LIST_HEAD(&aii->i_disks);
		INIT_LIST_HEAD(&aii->i_threads);
		spin_lock_init(&aii->i_lock);

		inode_init_once(&aii->vfs_inode);
	}
}

static int init_inodecache(void)
{
	asmfs_inode_cachep = kmem_cache_create("asmfs_inode_cache",
					       sizeof(struct asmfs_inode_info),
					       0, SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT,
					       init_once, NULL);

	if (asmfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	if (kmem_cache_destroy(asmfs_inode_cachep))
		printk(KERN_INFO "asmfs_inode_cache: not all structures were freed\n");
}

static int init_requestcache(void)
{
	asm_request_cachep = kmem_cache_create("asm_request",
				     	       sizeof(struct asm_request),
					       0, SLAB_HWCACHE_ALIGN,
					       NULL, NULL);
	if (asm_request_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_requestcache(void)
{
	if (kmem_cache_destroy(asm_request_cachep))
		printk(KERN_INFO "asm_request_cache: not all structures were freed\n");
}


/*
 * Disk file creation in the disks directory.
 */
static int asmfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct inode * inode;

	if (!S_ISBLK(mode))
		return -EINVAL;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;

	inode->i_ino = (unsigned long)inode;
	inode->i_mode = mode;
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_blocks = 0;
	inode->i_rdev = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	init_special_inode(inode, mode, dev);

	d_instantiate(dentry, inode);

	/* Extra count - pin the dentry in core */
	dget(dentry);

	return 0;
}

/*
 * Instance file creation in the iid directory.
 */
static int asmfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	struct inode *inode;

	if ((mode & S_IFMT) && !S_ISREG(mode))
		return -EINVAL;

	mode |= S_IFREG;

	inode = new_inode(dir->i_sb);
	if (!inode)
		return -ENOMEM;

	inode->i_ino = (unsigned long)inode;
	inode->i_mode = mode;
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_blocks = 0;
	inode->i_rdev = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &asmfs_file_inode_operations;
	inode->i_fop = &asmfs_file_operations;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;

	d_instantiate(dentry, inode);

	/* Extra count - pin the dentry in core */
	dget(dentry);

	return 0;
}

static void asmfs_put_super(struct super_block *sb)
{
	kfree(ASMFS_SB(sb));
}

enum {
	OPT_MAX_INSTANCES,
	OPT_ERR,
};

static match_table_t tokens = {
	{OPT_MAX_INSTANCES, "maxinstances=%d"},
	{OPT_ERR, NULL},
};

struct asmfs_params {
	long inodes;
};

static int parse_options(char * options, struct asmfs_params *p)
{
	char *s;
	substring_t args[MAX_OPT_ARGS];
	int option;

	p->inodes = -1;

	while ((s = strsep(&options,",")) != NULL) {
		int token;
		if (!*s)
			continue;
		token = match_token(s, tokens, args);
		switch (token) {
			case OPT_MAX_INSTANCES:
				if (match_int(&args[0], &option))
					return -EINVAL;
				p->inodes = option;
				break;

			default:
				return -EINVAL;
		}
	}

	return 0;
}

static void init_limits(struct asmfs_sb_info *asb, struct asmfs_params *p)
{
	struct sysinfo si;

	si_meminfo(&si);

	asb->max_inodes = 0;
	if (p->inodes >= 0)
		asb->max_inodes = p->inodes;

	asb->free_inodes = asb->max_inodes;

	return;
}

/* reset_limits is called during a remount to change the usage limits.

   This will suceed, even if the new limits are lower than current
   usage. This is the intended behaviour - new allocations will fail
   until usage falls below the new limit */
static void reset_limits(struct asmfs_sb_info *asb, struct asmfs_params *p)
{
	spin_lock_irq(&asb->asmfs_lock);

	if (p->inodes >= 0) {
		int used_inodes = asb->max_inodes - asb->free_inodes;

		asb->max_inodes = p->inodes;
		asb->free_inodes = asb->max_inodes - used_inodes;
	}

	spin_unlock_irq(&asb->asmfs_lock);
}

static int asmfs_remount(struct super_block * sb, int * flags, char * data)
{
	struct asmfs_params params;
	struct asmfs_sb_info * asb = ASMFS_SB(sb);

	if (parse_options((char *)data, &params) != 0)
		return -EINVAL;

	reset_limits(asb, &params);

	printk(KERN_DEBUG
 	       "ASM: oracleasmfs remounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "ASM:	maxinstances=%ld\n",
	       asb->max_inodes);

	return 0;
}

/*
 * Compute the maximum number of sectors the bdev can handle in one bio,
 * as a power of two.
 */
static int compute_max_sectors(struct block_device *bdev)
{
	int max_pages, max_sectors, pow_two_sectors;

	struct request_queue *q;

	q = bdev_get_queue(bdev);
	max_pages = q->max_sectors >> (PAGE_SHIFT - 9);
	if (max_pages > BIO_MAX_PAGES)
		max_pages = BIO_MAX_PAGES;
	if (max_pages > q->max_phys_segments)
		max_pages = q->max_phys_segments;
	if (max_pages > q->max_hw_segments)
		max_pages = q->max_hw_segments;
	max_pages--; /* Handle I/Os that straddle a page */

	max_sectors = max_pages << (PAGE_SHIFT - 9);

	/* Why is fls() 1-based???? */
	pow_two_sectors = 1 << (fls(max_sectors) - 1);

	return pow_two_sectors;
}

static int asm_open_disk(struct file *file, struct block_device *bdev)
{
	int ret;
	struct asm_disk_info *d;
	struct asm_disk_head *h;
	struct inode *inode = ASMFS_F2I(file);
	struct inode *disk_inode;

	ret = blkdev_get(bdev, FMODE_WRITE | FMODE_READ, 0);
	if (ret)
		goto out;

	ret = bd_claim(bdev, inode->i_sb);
	if (ret)
		goto out_get;

	ret = set_blocksize(bdev, bdev_hardsect_size(bdev));
	if (ret)
		goto out_claim;

	ret = -ENOMEM;
	h = kmalloc(sizeof(struct asm_disk_head), GFP_KERNEL);
	if (!h)
		goto out_claim;

	disk_inode = iget5_locked(asmdisk_mnt->mnt_sb,
				  (unsigned long)bdev, asmdisk_test,
				  asmdisk_set, ASMFS_I(inode));
	if (!disk_inode)
		goto out_head;

	d = ASMDISK_I(disk_inode);

	if (disk_inode->i_state & I_NEW) {
		if (atomic_read(&d->d_ios) != 0)
			BUG();
		if (d->d_live)
			BUG();

		disk_inode->i_mapping->backing_dev_info =
			&memory_backing_dev_info;
		d->d_bdev = bdev;
		d->d_max_sectors = compute_max_sectors(bdev);
		d->d_live = 1;
		unlock_new_inode(disk_inode);
	} else {
		/* Already claimed on first open */
		bd_release(bdev);
		blkdev_put(bdev);
	}

	h->h_disk = d;
	h->h_file = ASMFS_FILE(file);

	spin_lock_irq(&ASMFS_FILE(file)->f_lock);
	list_add(&h->h_flist, &ASMFS_FILE(file)->f_disks);
	spin_unlock_irq(&ASMFS_FILE(file)->f_lock);

	spin_lock_irq(&ASMFS_I(inode)->i_lock);
	list_add(&h->h_dlist, &d->d_open);
	spin_unlock_irq(&ASMFS_I(inode)->i_lock);

	return 0;

out_head:
	kfree(h);

out_claim:
	bd_release(bdev);

out_get:
	blkdev_put(bdev);

out:
	return ret;
}

static int asm_close_disk(struct file *file, unsigned long handle)
{
	struct inode *inode = ASMFS_F2I(file);
	struct asm_disk_info *d;
	struct block_device *bdev;
	struct inode *disk_inode;
	struct list_head *p;
	struct asm_disk_head *h;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	if (!ASMFS_FILE(file) || !ASMFS_I(inode))
		BUG();

	disk_inode = ilookup5(asmdisk_mnt->mnt_sb, handle,
			      asmdisk_test, ASMFS_I(inode));
	if (!disk_inode)
		return -EINVAL;

	d = ASMDISK_I(disk_inode);
	bdev = d->d_bdev;

	/*
	 * If an additional thread raced us to close the disk, it
	 * will have removed the disk from the list already.
	 */
	
	spin_lock_irq(&ASMFS_FILE(file)->f_lock);
	h = NULL;
	list_for_each(p, &ASMFS_FILE(file)->f_disks) {
		h = list_entry(p, struct asm_disk_head, h_flist);
		if (h->h_disk == d)
			break;
		h = NULL;
	}
	if (!h) {
		spin_unlock_irq(&ASMFS_FILE(file)->f_lock);
		iput(disk_inode);
		return -EINVAL;
	}
	list_del(&h->h_flist);
	spin_unlock_irq(&ASMFS_FILE(file)->f_lock);

	spin_lock_irq(&ASMFS_I(inode)->i_lock);
	list_del(&h->h_dlist);

	/* Last close */
	if (list_empty(&d->d_open))
	{
		LOG("Last close of disk 0x%p (%X) (0x%p, pid=%d)\n",
		    d->d_bdev, d->d_bdev->bd_dev, d, tsk->pid);

		/* I/O path can't look up this disk anymore */
		if (!d->d_live)
			BUG();
		d->d_live = 0;
		spin_unlock_irq(&ASMFS_I(inode)->i_lock);

		/* No need for a fast path */
		add_wait_queue(&ASMFS_FILE(file)->f_wait, &wait);
		do {
			set_task_state(tsk, TASK_UNINTERRUPTIBLE);

			if (!atomic_read(&d->d_ios))
				break;

			blk_run_address_space(bdev->bd_inode->i_mapping);
			/*
			 * Timeout of one second.  This is slightly
			 * subtle.  In this wait, and *only* this wait,
			 * we are waiting on I/Os that might have been
			 * initiated by another process.  In that case,
			 * the other process's afi will be signaled,
			 * not ours, so the wake_up() never happens
			 * here and we need the timeout.
			 */
#if 0  /* Damn you, kernel */
			io_schedule_timeout(HZ);
#else
			schedule_timeout(HZ);
#endif
		} while (1);
		set_task_state(tsk, TASK_RUNNING);
		remove_wait_queue(&ASMFS_FILE(file)->f_wait, &wait);
	}
	else
		spin_unlock_irq(&ASMFS_I(inode)->i_lock);

	kfree(h);

	/* Drop the ref from ilookup5() */
	iput(disk_inode);

	/* Real put */
	iput(disk_inode);

	return 0;
}  /* asm_close_disk() */


/* Timeout stuff ripped from aio.c - thanks Ben */
struct timeout {
	struct timer_list	timer;
	int			timed_out;
	wait_queue_head_t	wait;
};

static void timeout_func(unsigned long data)
{
	struct timeout *to = (struct timeout *)data;

	to->timed_out = 1;
	wake_up(&to->wait);
}

static inline void init_timeout(struct timeout *to)
{
	init_timer(&to->timer);
	to->timer.data = (unsigned long)to;
	to->timer.function = timeout_func;
	to->timed_out = 0;
	init_waitqueue_head(&to->wait);
}

static inline void set_timeout(struct timeout *to, const struct timespec *ts)
{
	unsigned long how_long;

	if (!ts->tv_sec && !ts->tv_nsec) {
		to->timed_out = 1;
		return;
	}

	how_long = ts->tv_sec * HZ;
#define HZ_NS (1000000000 / HZ)
	how_long += (ts->tv_nsec + HZ_NS - 1) / HZ_NS;
	
	to->timer.expires = jiffies + how_long;
	add_timer(&to->timer);
}

static inline void clear_timeout(struct timeout *to)
{
	del_timer_sync(&to->timer);
}

/* Must be called with asm_file_info->f_lock held */
static struct asm_disk_info *find_io_disk(struct file *file)
{
	struct asmfs_file_info *afi = ASMFS_FILE(file);
	struct asm_request *r;
	struct asm_disk_info *d = NULL;

	list_for_each_entry(r, &afi->f_ios, r_list) {
		d = r->r_disk;
		if (d) {
			igrab(&d->vfs_inode);
			break;
		}
	}

	return d;
}

static int asm_update_user_ioc(struct asm_request *r)
{
	asm_ioc __user *ioc;
	u16 tmp_status;

	ioc = r->r_ioc;
	dprintk("ASM: User IOC is 0x%p\n", ioc);

	/* Need to get the current userspace bits because ASM_CANCELLED is currently set there */
	dprintk("ASM: Getting tmp_status\n");
	if (get_user(tmp_status, &(ioc->status_asm_ioc)))
		return -EFAULT;
	r->r_status |= tmp_status;
	dprintk("ASM: Putting r_status (0x%08X)\n", r->r_status);
	if (put_user(r->r_status, &(ioc->status_asm_ioc)))
		return -EFAULT;
	if (r->r_status & ASM_ERROR) {
		dprintk("ASM: Putting r_error (0x%08X)\n", r->r_error);
		if (put_user(r->r_error, &(ioc->error_asm_ioc)))
			return -EFAULT;
	}
	if (r->r_status & ASM_COMPLETED) {
		if (put_user(r->r_elapsed, &(ioc->elaptime_asm_ioc)))
			return -EFAULT;
	}
	dprintk("ASM: r_status:0x%08X, bitmask:0x%08X, combined:0x%08X\n",
	       r->r_status,
	       (ASM_SUBMITTED | ASM_COMPLETED | ASM_ERROR),
	       (r->r_status & (ASM_SUBMITTED | ASM_COMPLETED | ASM_ERROR)));
	if (r->r_status & ASM_FREE) {
		u64 z = 0ULL;
		if (copy_to_user(&(ioc->reserved_asm_ioc),
				 &z, sizeof(ioc->reserved_asm_ioc)))
			return -EFAULT;
	} else if (r->r_status &
		   (ASM_SUBMITTED | ASM_ERROR)) {
		u64 key = (u64)(unsigned long)r;
		dprintk("ASM: Putting key 0x%p\n", r);
		/* Only on first submit */
		if (copy_to_user(&(ioc->reserved_asm_ioc),
				 &key, sizeof(ioc->reserved_asm_ioc)))
			return -EFAULT;
	}

	return 0;
}  /* asm_update_user_ioc() */


static struct asm_request *asm_request_alloc(void)
{
	struct asm_request *r;

	r = kmem_cache_alloc(asm_request_cachep, GFP_KERNEL);
	
	if (r) {
		r->r_status = ASM_SUBMITTED;
		r->r_error = 0;
		r->r_bio = NULL;
		r->r_elapsed = 0;
		r->r_disk = NULL;
	}

	return r;
}  /* asm_request_alloc() */


static void asm_request_free(struct asm_request *r)
{
	/* FIXME: Clean up bh and buffer stuff */

	kmem_cache_free(asm_request_cachep, r);
}  /* asm_request_free() */


static void asm_finish_io(struct asm_request *r)
{
	struct asm_disk_info *d;
	struct asmfs_file_info *afi = r->r_file;
	unsigned long flags;

	if (!afi)
		BUG();

	spin_lock_irqsave(&afi->f_lock, flags);

	if (r->r_bio) {
		r->r_bio->bi_private = afi->f_bio_free;
		afi->f_bio_free = r->r_bio;
		r->r_bio = NULL;
	}

 	d = r->r_disk;
	r->r_disk = NULL;

	list_del(&r->r_list);
	list_add(&r->r_list, &afi->f_complete);
	if (r->r_error)
		r->r_status |= ASM_ERROR;
	r->r_status |= ASM_COMPLETED;

	spin_unlock_irqrestore(&afi->f_lock, flags);

	if (d) {
		atomic_dec(&d->d_ios);
		if (atomic_read(&d->d_ios) < 0) {
			printk("ASM: d_ios underflow!\n");
			atomic_set(&d->d_ios, 0);
		}
	}

	r->r_elapsed = ((jiffies - r->r_elapsed) * 1000000) / HZ;

	wake_up(&afi->f_wait);
}  /* asm_finish_io() */


static void asm_end_ioc(struct asm_request *r, unsigned int bytes_done,
			int error)
{
	LOG_ENTRY();
	if (!r)
		BUG();

	if (!(r->r_status & ASM_SUBMITTED))
		BUG();

	LOG("ASM: Releasing kvec for request at 0x%p\n", r);
	LOG("ASM: asm_end_ioc(0x%p, %u, %d)\n", r, bytes_done,
	       error);
	LOG("ASM: asm_end_ioc bio len %u\n", bytes_done +
	       (r->r_bio ? r->r_bio->bi_size : 0));

	switch (error) {
		default:
			dprintk("ASM: Invalid error of %d!\n", err);
			r->r_error = ASM_ERR_INVAL;
			break;

		case 0:
			break;

		case -EFAULT:
			r->r_error = ASM_ERR_FAULT;
			break;

		case -EIO:
			r->r_error = ASM_ERR_IO;
			break;

		case -ENODEV:
			r->r_error = ASM_ERR_NODEV;
			break;

		case -ENOMEM:
			r->r_error = ASM_ERR_NOMEM;
			r->r_status |= ASM_LOCAL_ERROR;
			break;

		case -EINVAL:
			r->r_error = ASM_ERR_INVAL;
			break;
	}

	asm_finish_io(r);

	LOG_EXIT();
}  /* asm_end_ioc() */


static int asm_end_bio_io(struct bio *bio, unsigned int bytes_done,
			  int error)
{
	struct asm_request *r;

	LOG_ENTRY();

	r = bio->bi_private;

	LOG("ASM: Releasing bio for request at 0x%p\n", r);
	if (atomic_dec_and_test(&r->r_bio_count)) {
		asm_end_ioc(r, bytes_done, error);
	}

	LOG_EXIT_RET(0);

	return 0;
}  /* asm_end_bio_io() */


static int asm_submit_io(struct file *file,
			 asm_ioc __user *user_iocp,
			 asm_ioc *ioc)
{
	int ret, rw = READ;
	struct inode *inode = ASMFS_F2I(file);
	struct asm_request *r;
	struct asm_disk_info *d;
	struct inode *disk_inode;
	struct block_device *bdev;
	size_t count;

	LOG_ENTRY();
	if (!ioc)
		return -EINVAL;

	if (ioc->status_asm_ioc)
		return -EINVAL;

	r = asm_request_alloc();
	if (!r) {
		u16 status = ASM_FREE | ASM_ERROR | ASM_LOCAL_ERROR |
			ASM_BUSY;
		if (put_user(status, &(user_iocp->status_asm_ioc)))
			return -EFAULT;
		if (put_user(ASM_ERR_NOMEM, &(user_iocp->error_asm_ioc)))
			return -EFAULT;
		return 0;
	}

	LOG("ASM: New request at 0x%p alloc()ed for user ioc at 0x%p\n", r, user_iocp);

	r->r_file = ASMFS_FILE(file);
	r->r_ioc = user_iocp;  /* Userspace asm_ioc */

	spin_lock_irq(&ASMFS_FILE(file)->f_lock);
	list_add(&r->r_list, &ASMFS_FILE(file)->f_ios);
	spin_unlock_irq(&ASMFS_FILE(file)->f_lock);

	ret = -ENODEV;
	disk_inode = ilookup5(asmdisk_mnt->mnt_sb,
			      (unsigned long)ioc->disk_asm_ioc,
			      asmdisk_test, ASMFS_I(inode));
	if (!disk_inode)
		goto out_error;

	spin_lock_irq(&ASMFS_I(inode)->i_lock);

	d = ASMDISK_I(disk_inode);
	if (!d->d_live) {
		/* It's in the middle of closing */
		spin_unlock_irq(&ASMFS_I(inode)->i_lock);
		iput(disk_inode);
		goto out_error;
	}

	atomic_inc(&d->d_ios);
	r->r_disk = d;

	spin_unlock_irq(&ASMFS_I(inode)->i_lock);
	iput(disk_inode);

	bdev = d->d_bdev;

	count = ioc->rcount_asm_ioc * bdev_hardsect_size(bdev);

	LOG("ASM: first, 0x%llX; masked 0x%08lX\n",
	    (unsigned long long)ioc->first_asm_ioc,
	    (unsigned long)ioc->first_asm_ioc);
	/* linux only supports unsigned long size sector numbers */
	LOG("ASM: status: %u, buffer_asm_ioc: 0x%08lX, count: %lu\n",
	    ioc->status_asm_ioc,
	    (unsigned long)ioc->buffer_asm_ioc, (unsigned long)count);
	/* Note that priority is ignored for now */
	ret = -EINVAL;
	if (!ioc->buffer_asm_ioc ||
	    (ioc->buffer_asm_ioc != (unsigned long)ioc->buffer_asm_ioc) ||
	    (ioc->first_asm_ioc != (unsigned long)ioc->first_asm_ioc) ||
	    (ioc->rcount_asm_ioc != (unsigned long)ioc->rcount_asm_ioc) ||
	    (ioc->priority_asm_ioc > 7) ||
	    (count > (bdev_get_queue(bdev)->max_sectors << 9)) ||
	    (count < 0))
		goto out_error;

	/* Test device size, when known. (massaged from ll_rw_blk.c) */
	if (bdev->bd_inode->i_size >> 9) {
		sector_t maxsector = bdev->bd_inode->i_size >> 9;
		sector_t sector = (sector_t)ioc->first_asm_ioc;
		sector_t blks = (sector_t)ioc->rcount_asm_ioc;

		if (maxsector < blks || maxsector - blks < sector) {
			char b[BDEVNAME_SIZE];
			printk(KERN_INFO
			       "ASM: attempt to access beyond end of device\n");
			printk(KERN_INFO "ASM: dev %s: want=%llu, limit=%llu\n",
			       bdevname(bdev, b),
			       (unsigned long long)(sector + blks),
			       (unsigned long long)maxsector);
			goto out_error;
		}
	}


	LOG("ASM: Passed checks\n");

	switch (ioc->operation_asm_ioc) {
		default:
			goto out_error;
			break;

		case ASM_READ:
			rw = READ;
			break;

		case ASM_WRITE:
			rw = WRITE;
			break;

		case ASM_NOOP:
			/* Trigger an errorless completion */
			count = 0;
			break;
	}
	
	/* Not really an error, but hey, it's an end_io call */
	ret = 0;
	if (count == 0)
		goto out_error;

	ret = -ENOMEM;
	r->r_bio = bio_map_user(bdev_get_queue(bdev), bdev,
				(unsigned long)ioc->buffer_asm_ioc,
				count, rw == READ);
	if (IS_ERR(r->r_bio)) {
		ret = PTR_ERR(r->r_bio);
		r->r_bio = NULL;
		goto out_error;
	}

	r->r_bio->bi_sector = ioc->first_asm_ioc;

	/*
	 * If the bio is a bounced bio, we have to put the
	 * end_io on the child "real" bio
	 */
	r->r_bio->bi_end_io = asm_end_bio_io;
	r->r_bio->bi_private = r;

	r->r_elapsed = jiffies;  /* Set start time */

	atomic_set(&r->r_bio_count, 1);

	submit_bio(rw, r->r_bio);

out:
	ret = asm_update_user_ioc(r);

	LOG_EXIT_RET(ret);
	return ret;

out_error:
	LOG("out_error on request 0x%p\n", r);
	asm_end_ioc(r, 0, ret);
	goto out;
}  /* asm_submit_io() */


static int asm_maybe_wait_io(struct file *file,
			     asm_ioc *iocp,
			     struct timeout *to)
{
	long ret;
	u64 p;
	struct asmfs_file_info *afi = ASMFS_FILE(file);
	struct asm_request *r;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	DECLARE_WAITQUEUE(to_wait, tsk);

	LOG_ENTRY();
	if (copy_from_user(&p, &(iocp->reserved_asm_ioc),
			   sizeof(p)))
		return -EFAULT;

	LOG("ASM: User key is 0x%p\n",
		(struct asm_request *)(unsigned long)p);
	r = (struct asm_request *)(unsigned long)p;
	if (!r)
		return -EINVAL;

	spin_lock_irq(&afi->f_lock);
	/* Is it valid? It's surely ugly */
	if (!r->r_file || (r->r_file != afi) ||
	    list_empty(&r->r_list) || !(r->r_status & ASM_SUBMITTED)) {
		spin_unlock_irq(&afi->f_lock);
		return -EINVAL;
	}

	LOG("ASM: asm_request is valid...we think\n");
	if (!(r->r_status & (ASM_COMPLETED |
			     ASM_BUSY | ASM_ERROR))) {
		spin_unlock_irq(&afi->f_lock);
		add_wait_queue(&afi->f_wait, &wait);
		add_wait_queue(&to->wait, &to_wait);
		do {
			struct asm_disk_info *d;

			ret = 0;
			set_task_state(tsk, TASK_INTERRUPTIBLE);

			spin_lock_irq(&afi->f_lock);
			if (r->r_status & (ASM_COMPLETED |
					   ASM_BUSY | ASM_ERROR))
				break;
			d = r->r_disk;
			if (d)
				igrab(&d->vfs_inode);
			spin_unlock_irq(&afi->f_lock);

			if (d) {
				if (d->d_bdev)
					blk_run_address_space(d->d_bdev->bd_inode->i_mapping);
				iput(&d->vfs_inode);
			}

			ret = -ETIMEDOUT;
			if (to->timed_out)
				break;
			io_schedule();
			if (signal_pending(tsk)) {
				LOG("Signal pending\n");
				ret = -EINTR;
				break;
			}
		} while (1);
		set_task_state(tsk, TASK_RUNNING);
		remove_wait_queue(&afi->f_wait, &wait);
		remove_wait_queue(&to->wait, &to_wait);
		
		if (ret)
			return ret;
	}

	ret = 0;

	/* Somebody got here first */
	/*
	 * FIXME: This race means that we cannot be shared by two
	 * threads/processes (this struct file).  If everyone does
	 * their own open and gets their own struct file, this never
	 * happens and we're safe.
	 */
	if (r->r_status & ASM_FREE)
		goto out;  /* FIXME: Eek, holding lock */
	if (list_empty(&afi->f_complete))
		BUG();
#ifdef DEBUG
	{
		/* Check that the request is on afi->f_complete */
		struct list_head *p;
		struct asm_request *q;
		q = NULL;
		list_for_each(p, &afi->f_complete) {
			q = list_entry(p, struct asm_request, r_list);
			if (q == r)
				break;
			q = NULL;
		}
		if (!q)
			BUG();
	}
#endif  /* DEBUG */

	dprintk("ASM: Removing request 0x%p\n", r);
	list_del_init(&r->r_list);
	r->r_file = NULL;
	r->r_status |= ASM_FREE;

	spin_unlock_irq(&afi->f_lock);

	ret = asm_update_user_ioc(r);

	dprintk("ASM: Freeing request 0x%p\n", r);
	asm_request_free(r);

out:
	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io() */


static int asm_complete_io(struct file *file,
			   asm_ioc **ioc)
{
	int ret = 0;
	struct list_head *l;
	struct asm_request *r;
	struct asmfs_file_info *afi = ASMFS_FILE(file);

	LOG_ENTRY();
	spin_lock_irq(&afi->f_lock);

	if (list_empty(&afi->f_complete)) {
		spin_unlock_irq(&afi->f_lock);
		*ioc = NULL;
		LOG_EXIT_RET(0);
		return 0;
	}

	l = afi->f_complete.prev;
	r = list_entry(l, struct asm_request, r_list);
	list_del_init(&r->r_list);
	r->r_file = NULL;
	r->r_status |= ASM_FREE;

	spin_unlock_irq(&afi->f_lock);

	*ioc = r->r_ioc;
	
#ifdef DEBUG
	if (!(r->r_status & (ASM_COMPLETED | ASM_ERROR)))
		BUG();
#endif /* DEBUG */

	ret = asm_update_user_ioc(r);

	asm_request_free(r);

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_complete_io() */


static int asm_wait_completion(struct file *file,
			       struct oracleasm_io_v2 *io,
			       struct timeout *to,
			       u32 *status)
{
	int ret;
	struct asmfs_file_info *afi = ASMFS_FILE(file);
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	DECLARE_WAITQUEUE(to_wait, tsk);

	LOG_ENTRY();
	/* Early check - expensive stuff follows */
	ret = -ETIMEDOUT;
	if (to->timed_out)
		goto out;

	spin_lock_irq(&afi->f_lock);
	if (list_empty(&afi->f_ios) &&
	    list_empty(&afi->f_complete))
	{
		/* No I/Os left */
		spin_unlock_irq(&afi->f_lock);
		ret = 0;
		*status |= ASM_IO_IDLE;
		goto out;
	}
	spin_unlock_irq(&afi->f_lock);

	add_wait_queue(&afi->f_wait, &wait);
	add_wait_queue(&to->wait, &to_wait);
	do {
		struct asm_disk_info *d;

		ret = 0;
		set_task_state(tsk, TASK_INTERRUPTIBLE);

		spin_lock_irq(&afi->f_lock);
		if (!list_empty(&afi->f_complete)) {
			spin_unlock_irq(&afi->f_lock);
			break;
		}

		d = find_io_disk(file);
		spin_unlock_irq(&afi->f_lock);

		if (d) {
			if (d->d_bdev)
				blk_run_address_space(d->d_bdev->bd_inode->i_mapping);
			iput(&d->vfs_inode);
		}

		ret = -ETIMEDOUT;
		if (to->timed_out)
			break;
		io_schedule();
		if (signal_pending(tsk)) {
			ret = -EINTR;
			break;
		}
	} while (1);
	set_task_state(tsk, TASK_RUNNING);
	remove_wait_queue(&afi->f_wait, &wait);
	remove_wait_queue(&to->wait, &to_wait);

out:
	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_wait_completion() */


static inline int asm_submit_io_native(struct file *file,
       				       struct oracleasm_io_v2 *io)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;
	asm_ioc tmp;

	LOG_ENTRY();
	for (i = 0; i < io->io_reqlen; i++) {
		ret = -EFAULT;
		if (get_user(iocp,
			     ((asm_ioc **)((unsigned long)(io->io_requests))) + i))
			break;

		if (copy_from_user(&tmp, iocp, sizeof(tmp)))
			break;

		LOG("ASM: Submitting user ioc at 0x%p\n", iocp);
		ret = asm_submit_io(file, iocp, &tmp);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_submit_io_native() */


static inline int asm_maybe_wait_io_native(struct file *file,
					   struct oracleasm_io_v2 *io,
					   struct timeout *to)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_waitlen; i++) {
		if (get_user(iocp,
			     ((asm_ioc **)((unsigned long)(io->io_waitreqs))) + i))
			return -EFAULT;

		ret = asm_maybe_wait_io(file, iocp, to);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io_native() */


static inline int asm_complete_ios_native(struct file *file,
					  struct oracleasm_io_v2 *io,
					  struct timeout *to,
					  u32 *status)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_complen; i++) {
		ret = asm_complete_io(file, &iocp);
		if (ret)
			break;
		if (iocp) {
			ret = put_user(iocp,
				       ((asm_ioc **)((unsigned long)(io->io_completions))) + i);
			       if (ret)
				   break;
			continue;
		}

		/* We had waiters that are full */
		if (*status & ASM_IO_WAITED)
			break;

		ret = asm_wait_completion(file, io, to, status);
		if (ret)
			break;
		if (*status & ASM_IO_IDLE)
			break;

		i--; /* Reset this completion */

	}

	LOG_EXIT_RET(ret ? ret : i);
	return (ret ? ret : i);
}  /* asm_complete_ios_native() */


#if BITS_PER_LONG == 64
static inline void asm_promote_64(asm_ioc64 *ioc)
{
	asm_ioc32 *ioc_32 = (asm_ioc32 *)ioc;

	LOG_ENTRY();

	/*
	 * Promote the 32bit pointers at the end of the asm_ioc32
	 * into the asm_ioc64.
	 *
	 * Promotion must be done from the tail backwards.
	 */
	LOG("Promoting (0x%X, 0x%X) to ",
	    ioc_32->check_asm_ioc,
	    ioc_32->buffer_asm_ioc);
	ioc->check_asm_ioc = (u64)ioc_32->check_asm_ioc;
	ioc->buffer_asm_ioc = (u64)ioc_32->buffer_asm_ioc;
	LOG("(0x%llX, 0x%llX)\n",
	    ioc->check_asm_ioc,
	    ioc->buffer_asm_ioc);

	LOG_EXIT();
}  /* asm_promote_64() */


static inline int asm_submit_io_thunk(struct file *file,
	       			      struct oracleasm_io_v2 *io)
{
	int ret = 0;
	u32 i;
	u32 iocp_32;
	asm_ioc32 *iocp;
	asm_ioc tmp;

	LOG_ENTRY();
	for (i = 0; i < io->io_reqlen; i++) {
		ret = -EFAULT;
		/*
		 * io->io_requests is an asm_ioc32**, but the pointers
		 * are 32bit pointers.
		 */
		if (get_user(iocp_32,
			     ((u32 *)((unsigned long)(io->io_requests))) + i))
			break;

		iocp = (asm_ioc32 *)(unsigned long)iocp_32;

		if (copy_from_user(&tmp, iocp, sizeof(*iocp)))
			break;

		asm_promote_64(&tmp);

		ret = asm_submit_io(file, (asm_ioc *)iocp, &tmp);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_submit_io_thunk() */


static inline int asm_maybe_wait_io_thunk(struct file *file,
					  struct oracleasm_io_v2 *io,
					  struct timeout *to)
{
	int ret = 0;
	u32 i;
	u32 iocp_32;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_waitlen; i++) {
		/*
		 * io->io_waitreqs is an asm_ioc32**, but the pointers
		 * are 32bit pointers.
		 */
		if (get_user(iocp_32,
			     ((u32 *)((unsigned long)(io->io_waitreqs))) + i))
			return -EFAULT;

		/* Remember, the this is pointing to 32bit userspace */
		iocp = (asm_ioc *)(unsigned long)iocp_32;

		ret = asm_maybe_wait_io(file, iocp, to);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io_thunk() */


static inline int asm_complete_ios_thunk(struct file *file,
					 struct oracleasm_io_v2 *io,
					 struct timeout *to,
					 u32 *status)
{
	int ret = 0;
	u32 i;
	u32 iocp_32;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_complen; i++) {
		ret = asm_complete_io(file, &iocp);
		if (ret)
			break;
		if (iocp) {
			iocp_32 = (u32)(unsigned long)iocp;

			ret = put_user(iocp_32,
				       ((u32 *)((unsigned long)(io->io_completions))) + i);
			       if (ret)
				   break;
			continue;
		}

		/* We had waiters that are full */
		if (*status & ASM_IO_WAITED)
			break;

		ret = asm_wait_completion(file, io, to, status);
		if (ret)
			break;
		if (*status & ASM_IO_IDLE)
			break;

		i--; /* Reset this completion */
	}

	LOG_EXIT_RET(ret ? ret : i);
	return (ret ? ret : i);
}  /* asm_complete_ios_thunk() */

#endif  /* BITS_PER_LONG == 64 */


static int asm_do_io(struct file *file, struct oracleasm_io_v2 *io,
		     int bpl)
{
	int ret = 0;
	u32 status = 0;
	struct timeout to;

	LOG_ENTRY();

	init_timeout(&to);

	if (io->io_timeout) {
		struct timespec ts;

		LOG("ASM: asm_do_io() was passed a timeout\n");
		ret = -EFAULT;
		if (copy_from_user(&ts,
				   (struct timespec *)(unsigned long)(io->io_timeout),
				   sizeof(ts)))
			goto out;

		set_timeout(&to, &ts);
		if (to.timed_out) {
			io->io_timeout = (u64)0;
			clear_timeout(&to);
		}
	}

	ret = 0;
	if (io->io_requests) {
		LOG("ASM: oracleasm_io has requests; reqlen %d\n",
		    io->io_reqlen);
		ret = -EINVAL;
		if (bpl == ASM_BPL_32)
			ret = asm_submit_io_32(file, io);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_submit_io_64(file, io);
#endif  /* BITS_PER_LONG == 64 */

		if (ret)
			goto out_to;
	}

	if (io->io_waitreqs) {
		LOG("ASM: oracleasm_io has waits; waitlen %d\n",
		    io->io_waitlen);
		ret = -EINVAL;
		if (bpl == ASM_BPL_32)
			ret = asm_maybe_wait_io_32(file, io, &to);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_maybe_wait_io_64(file, io, &to);
#endif  /* BITS_PER_LONG == 64 */

		if (ret)
			goto out_to;

		status |= ASM_IO_WAITED;
	}

	if (io->io_completions) {
		LOG("ASM: oracleasm_io has completes; complen %d\n",
		    io->io_complen);
		ret = -EINVAL;
		if (bpl == ASM_BPL_32)
			ret = asm_complete_ios_32(file, io, &to,
						  &status);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_complete_ios_64(file, io, &to,
						  &status);
#endif  /* BITS_PER_LONG == 64 */

		if (ret < 0)
			goto out_to;
		if (ret >= io->io_complen)
			status |= ASM_IO_FULL;
		ret = 0;
	}

out_to:
	if (io->io_timeout)
		clear_timeout(&to);

out:
	if (put_user(status, (u32 *)(unsigned long)(io->io_statusp)))
		ret = -EFAULT;
	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_do_io() */

static void asm_cleanup_bios(struct file *file)
{
	struct asmfs_file_info *afi = ASMFS_FILE(file);
	struct bio *bio;

	while (afi->f_bio_free) {
		spin_lock_irq(&afi->f_lock);
		bio = afi->f_bio_free;
		afi->f_bio_free = bio->bi_private;
		spin_unlock_irq(&afi->f_lock);

		bio_unmap_user(bio);
	}
}

static int asmfs_file_open(struct inode * inode, struct file * file)
{
	struct asmfs_inode_info * aii;
	struct asmfs_file_info * afi;

	if (ASMFS_FILE(file))
		BUG();

	afi = (struct asmfs_file_info *)kmalloc(sizeof(*afi),
						GFP_KERNEL);
	if (!afi)
		return -ENOMEM;
	afi->f_file = file;
	afi->f_bio_free = NULL;
	spin_lock_init(&afi->f_lock);
	INIT_LIST_HEAD(&afi->f_ctx);
	INIT_LIST_HEAD(&afi->f_disks);
	INIT_LIST_HEAD(&afi->f_ios);
	INIT_LIST_HEAD(&afi->f_complete);
	init_waitqueue_head(&afi->f_wait);

	aii = ASMFS_I(ASMFS_F2I(file));
	spin_lock_irq(&aii->i_lock);
	list_add(&afi->f_ctx, &aii->i_threads);
	spin_unlock_irq(&aii->i_lock);

	file->private_data = afi;

	return 0;
}  /* asmfs_file_open() */


static int asmfs_file_release(struct inode *inode, struct file *file)
{
	struct asmfs_inode_info *aii;
	struct asmfs_file_info *afi;
	struct asm_disk_head *h, *n;
	struct list_head *p;
	struct asm_disk_info *d;
	struct asm_request *r;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	aii = ASMFS_I(ASMFS_F2I(file));
	afi = ASMFS_FILE(file);

	if (!afi)
		BUG();

	/*
	 * Shouldn't need the lock, no one else has a reference
	 * asm_close_disk will need to take it when completing I/O
	 */
	list_for_each_entry_safe(h, n, &afi->f_disks, h_flist) {
		d = h->h_disk;
		asm_close_disk(file, (unsigned long)d->d_bdev);
	}

	/* FIXME: Clean up things that hang off of afi */

	spin_lock_irq(&aii->i_lock);
	list_del(&afi->f_ctx);
	spin_unlock_irq(&aii->i_lock);

	/* No need for a fastpath */
	add_wait_queue(&afi->f_wait, &wait);
	do {
		struct asm_disk_info *d;

		set_task_state(tsk, TASK_UNINTERRUPTIBLE);

		spin_lock_irq(&afi->f_lock);
		if (list_empty(&afi->f_ios))
		    break;

		d = find_io_disk(file);
		spin_unlock_irq(&afi->f_lock);

		if (d) {
			if (d->d_bdev)
				blk_run_address_space(d->d_bdev->bd_inode->i_mapping);
			iput(&d->vfs_inode);
		}

		dprintk("There are still I/Os hanging off of afi 0x%p\n",
		       afi);
		io_schedule();
	} while (1);
	set_task_state(tsk, TASK_RUNNING);
	remove_wait_queue(&afi->f_wait, &wait);
	
	/* I don't *think* we need the lock here anymore, but... */

	/* Clear unreaped I/Os */
	while (!list_empty(&afi->f_complete)) {
		p = afi->f_complete.prev;
		r = list_entry(p, struct asm_request, r_list);
		list_del(&r->r_list);
		r->r_file = NULL;
		asm_request_free(r);
	}
	spin_unlock_irq(&afi->f_lock);

	/* And cleanup any pages from those I/Os */
	asm_cleanup_bios(file);

	dprintk("ASM: Done with afi 0x%p\n", afi);
	file->private_data = NULL;
	kfree(afi);

	return 0;
}  /* asmfs_file_release() */

/*
 * Verify that the magic and ABI versions are valid.  Future
 * drivers might support more than one ABI version, so ESRCH is returned
 * for "valid ABI version not found"
 */
static int asmfs_verify_abi(struct oracleasm_abi_info *abi_info)
{
	if (abi_info->ai_magic != ASM_ABI_MAGIC)
		return -EBADR;
	if (abi_info->ai_version != ASM_ABI_VERSION)
		return -ESRCH;

	return 0;
}

static ssize_t asmfs_svc_query_version(struct file *file, char *buf, size_t size)
{
	struct oracleasm_abi_info *abi_info;
	int ret;

	if (size != sizeof(struct oracleasm_abi_info))
		return -EINVAL;

       	abi_info = (struct oracleasm_abi_info *)buf;

	ret = asmfs_verify_abi(abi_info);
	if (ret) {
	       	if (ret == -ESRCH) {
			abi_info->ai_version = ASM_ABI_VERSION;
			abi_info->ai_status = -ESRCH;
		} else
			goto out;
	}

	ret = -EBADR;
	if (abi_info->ai_size != sizeof(struct oracleasm_abi_info))
		goto out;
	ret = -EBADRQC;
	if (abi_info->ai_type != ASMOP_QUERY_VERSION)
		goto out;

	ret = 0;

out:
	if (!abi_info->ai_status)
		abi_info->ai_status = ret;
	
	return size;
}

static ssize_t asmfs_svc_get_iid(struct file *file, char *buf, size_t size)
{
	struct oracleasm_get_iid_v2 *iid_info;
	struct asmfs_sb_info *asb = ASMFS_SB(ASMFS_F2I(file)->i_sb);
	int ret;

	if (size != sizeof(struct oracleasm_get_iid_v2))
		return -EINVAL;

	iid_info = (struct oracleasm_get_iid_v2 *)buf;

	ret = asmfs_verify_abi(&iid_info->gi_abi);
	if (ret)
		goto out;
	ret = -EBADR;
	if (iid_info->gi_abi.ai_size !=
	    sizeof(struct oracleasm_get_iid_v2))
		goto out;
	ret = -EBADRQC;
	if (iid_info->gi_abi.ai_type != ASMOP_GET_IID)
		goto out;

	spin_lock_irq(&asb->asmfs_lock);
	iid_info->gi_iid = (u64)asb->next_iid;
	asb->next_iid++;
	spin_unlock_irq(&asb->asmfs_lock);

	ret = 0;

out:
	iid_info->gi_abi.ai_status = ret;

	return size;
}

static ssize_t asmfs_svc_check_iid(struct file *file, char *buf, size_t size)
{
	struct oracleasm_get_iid_v2 *iid_info;
	struct asmfs_sb_info *asb = ASMFS_SB(ASMFS_F2I(file)->i_sb);
	int ret;

	if (size != sizeof(struct oracleasm_get_iid_v2))
		return -EINVAL;

	iid_info = (struct oracleasm_get_iid_v2 *)buf;

	ret = asmfs_verify_abi(&iid_info->gi_abi);
	if (ret)
		goto out;

	ret = -EBADR;
	if (iid_info->gi_abi.ai_size !=
	    sizeof(struct oracleasm_get_iid_v2))
		goto out;
	ret = -EBADRQC;
	if (iid_info->gi_abi.ai_type != ASMOP_CHECK_IID)
		goto out;

	spin_lock_irq(&asb->asmfs_lock);
	if (iid_info->gi_iid >= (u64)asb->next_iid)
		iid_info->gi_iid = (u64)0;
	spin_unlock_irq(&asb->asmfs_lock);

	ret = 0;

out:
	iid_info->gi_abi.ai_status = ret;

	return size;
}

static ssize_t asmfs_svc_query_disk(struct file *file, char *buf, size_t size)
{
	struct oracleasm_query_disk_v2 *qd_info;
	struct file *filp;
	struct block_device *bdev;
	int ret;

	if (size != sizeof(struct oracleasm_query_disk_v2))
		return -EINVAL;

	qd_info = (struct oracleasm_query_disk_v2 *)buf;

	ret = asmfs_verify_abi(&qd_info->qd_abi);
	if (ret)
		goto out;

	ret = -EBADR;
	if (qd_info->qd_abi.ai_size !=
	    sizeof(struct oracleasm_query_disk_v2))
		goto out;
	ret = -EBADRQC;
	if (qd_info->qd_abi.ai_type != ASMOP_QUERY_DISK)
		goto out;

	ret = -ENODEV;
	filp = fget(qd_info->qd_fd);
	if (!filp)
		goto out;

	bdev = I_BDEV(filp->f_mapping->host);

	qd_info->qd_max_sectors = compute_max_sectors(bdev);
	qd_info->qd_hardsect_size = bdev_hardsect_size(bdev);

	fput(filp);

	ret = 0;

out:
	qd_info->qd_abi.ai_status = ret;

	return size;
}

static ssize_t asmfs_svc_open_disk(struct file *file, char *buf, size_t size)
{
	struct oracleasm_open_disk_v2 od_info;
	struct block_device *bdev = NULL;
	struct file *filp;
	int ret;

	if (size != sizeof(struct oracleasm_open_disk_v2))
		return -EINVAL;

	if (copy_from_user(&od_info,
			   (struct oracleasm_open_disk_v2 __user *)buf,
			   sizeof(struct oracleasm_open_disk_v2)))
		return -EFAULT;

	od_info.od_handle = 0; /* Unopened */

	ret = asmfs_verify_abi(&od_info.od_abi);
	if (ret)
		goto out_error;

	ret = -EBADR;
	if (od_info.od_abi.ai_size !=
	    sizeof(struct oracleasm_open_disk_v2))
		goto out_error;
	ret = -EBADRQC;
	if (od_info.od_abi.ai_type != ASMOP_OPEN_DISK)
		goto out_error;

	ret = -ENODEV;
	filp = fget(od_info.od_fd);
	if (!filp)
		goto out_error;

	if (igrab(filp->f_mapping->host)) {
		bdev = I_BDEV(filp->f_mapping->host);

		ret = asm_open_disk(file, bdev);
	}
	fput(filp);
	if (ret)
		goto out_error;

	od_info.od_handle = (u64)(unsigned long)bdev;
out_error:
	od_info.od_abi.ai_status = ret;
	if (copy_to_user((struct oracleasm_open_disk_v2 __user *)buf,
			 &od_info,
			 sizeof(struct oracleasm_open_disk_v2))) {
		if (od_info.od_handle)
			asm_close_disk(file,
				       (unsigned long)od_info.od_handle);
		/* Ignore close errors, this is the real error */
		return -EFAULT;
	}

	return size;
}

static ssize_t asmfs_svc_close_disk(struct file *file, char *buf, size_t size)
{
	struct oracleasm_close_disk_v2 cd_info;
	int ret;

	if (size != sizeof(struct oracleasm_close_disk_v2))
		return -EINVAL;

	if (copy_from_user(&cd_info,
			   (struct oracleasm_close_disk_v2 __user *)buf,
			   sizeof(struct oracleasm_close_disk_v2)))
		return -EFAULT;

	ret = asmfs_verify_abi(&cd_info.cd_abi);
	if (ret)
		goto out_error;

	ret = -EBADR;
	if (cd_info.cd_abi.ai_size !=
	    sizeof(struct oracleasm_close_disk_v2))
		goto out_error;
	ret = -EBADRQC;
	if (cd_info.cd_abi.ai_type != ASMOP_CLOSE_DISK)
		goto out_error;

	ret = asm_close_disk(file, (unsigned long)cd_info.cd_handle);

out_error:
	cd_info.cd_abi.ai_status = ret;
	if (copy_to_user((struct oracleasm_close_disk_v2 __user *)buf,
			 &cd_info,
			 sizeof(struct oracleasm_close_disk_v2)))
		return -EFAULT;

	return size;
}

static ssize_t asmfs_svc_io32(struct file *file, char *buf, size_t size)
{
	struct oracleasm_abi_info __user *user_abi_info;
	struct oracleasm_io_v2 io_info;
	int ret;

	if (size != sizeof(struct oracleasm_io_v2))
		return -EINVAL;

	if (copy_from_user(&io_info,
			   (struct oracleasm_io_v2 __user *)buf,
			   sizeof(struct oracleasm_io_v2)))
		return -EFAULT;

	ret = asmfs_verify_abi(&io_info.io_abi);
	if (ret)
		goto out_error;

	ret = -EBADR;
	if (io_info.io_abi.ai_size !=
	    sizeof(struct oracleasm_io_v2))
		goto out_error;
	ret = -EBADRQC;
	if (io_info.io_abi.ai_type != ASMOP_IO32)
		goto out_error;

	ret = asm_do_io(file, &io_info, ASM_BPL_32);

out_error:
	user_abi_info = (struct oracleasm_abi_info __user *)buf;
	if (put_user(ret, &(user_abi_info->ai_status)))
		return -EFAULT;

	return size;
}

#if BITS_PER_LONG == 64
static ssize_t asmfs_svc_io64(struct file *file, char *buf, size_t size)
{
	struct oracleasm_abi_info __user *user_abi_info;
	struct oracleasm_io_v2 io_info;
	int ret;

	if (size != sizeof(struct oracleasm_io_v2))
		return -EINVAL;

	if (copy_from_user(&io_info,
			   (struct oracleasm_io_v2 __user *)buf,
			   sizeof(struct oracleasm_io_v2)))
		return -EFAULT;

	ret = asmfs_verify_abi(&io_info.io_abi);
	if (ret)
		goto out_error;

	ret = -EBADR;
	if (io_info.io_abi.ai_size !=
	    sizeof(struct oracleasm_io_v2))
		goto out_error;
	ret = -EBADRQC;
	if (io_info.io_abi.ai_type != ASMOP_IO64)
		goto out_error;

	ret = asm_do_io(file, &io_info, ASM_BPL_64);

out_error:
	user_abi_info = (struct oracleasm_abi_info __user *)buf;
	if (put_user(ret, &(user_abi_info->ai_status)))
		return -EFAULT;

	return size;
}
#endif  /* BITS_PER_LONG == 64 */


/*
 * Because each of these operations need to access the filp->private,
 * we must multiplex.
 */
static ssize_t asmfs_file_read(struct file *file, char *buf, size_t size, loff_t *pos)
{
	struct oracleasm_abi_info __user *user_abi_info;
	ssize_t ret;
	int op;

	asm_cleanup_bios(file);

	user_abi_info = (struct oracleasm_abi_info __user *)buf;
	if (get_user(op, &((user_abi_info)->ai_type)))
		return -EFAULT;

	switch (op) {
		default:
			ret = -EBADRQC;
			break;

		case ASMOP_OPEN_DISK:
			ret = asmfs_svc_open_disk(file, (char *)buf,
						  size);
			break;

		case ASMOP_CLOSE_DISK:
			ret = asmfs_svc_close_disk(file, (char *)buf,
						   size);
			break;

		case ASMOP_IO32:
			ret = asmfs_svc_io32(file, (char *)buf, size);
			break;

#if BITS_PER_LONG == 64
		case ASMOP_IO64:
			ret = asmfs_svc_io64(file, (char *)buf, size);
			break;
#endif  /* BITS_PER_LONG == 64 */
	}

	return ret;
}

static struct file_operations asmfs_file_operations = {
	.open		= asmfs_file_open,
	.release	= asmfs_file_release,
	.read		= asmfs_file_read,
};

static struct inode_operations asmfs_file_inode_operations = {
	.getattr	= simple_getattr,
};

/*  See init_asmfs_dir_operations() */
static struct file_operations asmfs_dir_operations = {0, };

static struct inode_operations asmfs_disk_dir_inode_operations = {
	.lookup		= simple_lookup,
	.unlink		= simple_unlink,
	.mknod		= asmfs_mknod,
};
static struct inode_operations asmfs_iid_dir_inode_operations = {
	.create		= asmfs_create,
	.lookup		= simple_lookup,
	.unlink		= simple_unlink,
};

static struct super_operations asmfs_ops = {
	.statfs		= simple_statfs,
	.alloc_inode	= asmfs_alloc_inode,
	.destroy_inode	= asmfs_destroy_inode,
	.drop_inode	= generic_delete_inode,
	/* These last three only required for limited maxinstances */
	.put_super	= asmfs_put_super,
	.remount_fs     = asmfs_remount,
};

/*
 * Initialisation
 */

static int asmfs_fill_super(struct super_block *sb,
			    void *data, int silent)
{
	struct inode *inode;
	struct dentry *root, *dentry;
	struct asmfs_sb_info *asb;
	struct asmfs_params params;
	struct qstr name;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = ASMFS_MAGIC;
	sb->s_op = &asmfs_ops;
	sb->s_maxbytes = MAX_NON_LFS;	/* Why? */

	asb = kmalloc(sizeof(struct asmfs_sb_info), GFP_KERNEL);
	if (!asb)
		return -ENOMEM;
	sb->s_fs_info = asb;

	asb->asmfs_super = sb;
	asb->next_iid = 1;
	spin_lock_init(&asb->asmfs_lock);

	if (parse_options((char *)data, &params) != 0)
		goto out_free_asb;

	init_limits(asb, &params);

	inode = new_inode(sb);
	if (!inode)
		goto out_free_asb;

	inode->i_ino = (unsigned long)inode;
	inode->i_mode = S_IFDIR | 0755;
	inode->i_uid = inode->i_gid = 0;
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_blocks = 0;
	inode->i_rdev = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &asmfs_dir_operations;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	/* directory inodes start off with i_nlink == 2 (for "." entry) */
	inode->i_nlink++;

	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		goto out_free_asb;
	}

	name.name = ASM_MANAGER_DISKS;
	name.len = strlen(ASM_MANAGER_DISKS);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_inode(sb);
	if (!inode)
		goto out_genocide;
	inode->i_ino = (unsigned long)inode;
	inode->i_mode = S_IFDIR | 0755;
	inode->i_uid = inode->i_gid = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &asmfs_disk_dir_inode_operations;
	inode->i_fop = &asmfs_dir_operations;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	name.name = ASM_MANAGER_INSTANCES;
	name.len = strlen(ASM_MANAGER_INSTANCES);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_inode(sb);
	if (!inode)
		goto out_genocide;
	inode->i_ino = (unsigned long)inode;
	inode->i_mode = S_IFDIR | 0770;
	inode->i_uid = inode->i_gid = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &asmfs_iid_dir_inode_operations;
	inode->i_fop = &asmfs_dir_operations;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	name.name = asm_operation_files[ASMOP_QUERY_VERSION];
	name.len = strlen(asm_operation_files[ASMOP_QUERY_VERSION]);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_transaction_inode(sb, 0770,
				      &trans_contexts[ASMOP_QUERY_VERSION]);
	if (!inode)
		goto out_genocide;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	name.name = asm_operation_files[ASMOP_GET_IID];
	name.len = strlen(asm_operation_files[ASMOP_GET_IID]);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_transaction_inode(sb, 0770,
				      &trans_contexts[ASMOP_GET_IID]);
	if (!inode)
		goto out_genocide;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	name.name = asm_operation_files[ASMOP_CHECK_IID];
	name.len = strlen(asm_operation_files[ASMOP_CHECK_IID]);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_transaction_inode(sb, 0770,
				      &trans_contexts[ASMOP_CHECK_IID]);
	if (!inode)
		goto out_genocide;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	name.name = asm_operation_files[ASMOP_QUERY_DISK];
	name.len = strlen(asm_operation_files[ASMOP_QUERY_DISK]);
	name.hash = full_name_hash(name.name, name.len);
	dentry = d_alloc(root, &name);
	if (!dentry)
		goto out_genocide;
	inode = new_transaction_inode(sb, 0770,
				      &trans_contexts[ASMOP_QUERY_DISK]);
	if (!inode)
		goto out_genocide;
	inode->i_mapping->backing_dev_info = &memory_backing_dev_info;
	d_add(dentry, inode);

	sb->s_root = root;


	printk(KERN_DEBUG "ASM: oracleasmfs mounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "ASM:	maxinstances=%ld\n", asb->max_inodes);
	return 0;

out_genocide:
	d_genocide(root);
	dput(root);

out_free_asb:
	sb->s_fs_info = NULL;
	kfree(asb);

	return -EINVAL;
}


static void __init init_asmfs_dir_operations(void) {
	asmfs_dir_operations		= simple_dir_operations;
	asmfs_dir_operations.fsync	= simple_sync_file;
};


static struct super_block *asmfs_get_sb(struct file_system_type *fs_type,
					int flags, const char *dev_name,
					void *data)
{
	return get_sb_nodev(fs_type, flags, data, asmfs_fill_super);
}

static struct file_system_type asmfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "oracleasmfs",
	.get_sb		= asmfs_get_sb,
	.kill_sb	= kill_litter_super,
};

static int __init init_asmfs_fs(void)
{
	int ret;

	LOG("sizeof asm_ioc32: %lu\n",
	    (unsigned long)sizeof(asm_ioc32));
	ret = init_inodecache();
	if (ret) {
		printk("oracleasmfs: Unable to create asmfs_inode_cache\n");
		goto out_inodecache;
	}

	ret = init_requestcache();
	if (ret) {
		printk("oracleasmfs: Unable to create asm_request cache\n");
		goto out_requestcache;
	}

	ret = init_asmdiskcache();
	if (ret) {
		printk("oracleasmfs: Unable to initialize the disk cache\n");
		goto out_diskcache;
	}

	init_asmfs_dir_operations();
	ret = register_filesystem(&asmfs_fs_type);
	if (ret) {
		printk("oracleasmfs: Unable to register filesystem\n");
		goto out_register;
	}

	return 0;

out_register:
	destroy_asmdiskcache();

out_diskcache:
	destroy_requestcache();

out_requestcache:
	destroy_inodecache();

out_inodecache:
	return ret;
}

static void __exit exit_asmfs_fs(void)
{
	unregister_filesystem(&asmfs_fs_type);
	destroy_asmdiskcache();
	destroy_requestcache();
	destroy_inodecache();
}

module_init(init_asmfs_fs)
module_exit(exit_asmfs_fs)
MODULE_LICENSE("GPL");
MODULE_VERSION(ASM_MODULE_VERSION);
MODULE_AUTHOR("Joel Becker <joel.becker@oracle.com>");
MODULE_DESCRIPTION("Kernel driver backing the Generic Linux ASM Library.");
