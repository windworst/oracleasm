/*
 * osm - Kernel driver for the Oracle Storage Manager
 *
 * Copyright (C) 2002,2003 Oracle Corporation.  All rights reserved.
 *
 * This driver's filesystem code is based on the ramfs filesystem.
 * Copyright information for the original source appears below.  This
 * file is released under the GNU General Public License.
 */

/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *		 2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/blk.h>
#include <linux/blkdev.h>

#include <asm/uaccess.h>
#include <linux/spinlock.h>

#include "linux/osmcompat32.h"
#include "linux/osmkernel.h"
#include "linux/osmabi.h"
#include "linux/osmdisk.h"
#include "osmerror.h"


#if PAGE_CACHE_SIZE % 1024
#error Oh no, PAGE_CACHE_SIZE is not divisible by 1k! I cannot cope.
#endif  /* PAGE_CACHE_SIZE % 1024 */

#define IBLOCKS_PER_PAGE  (PAGE_CACHE_SIZE / 512)
#define K_PER_PAGE (PAGE_CACHE_SIZE / 1024)

/* some random number */
#define ASMFS_MAGIC	0x958459f6

/*
 * Max I/O is 64K.  Why 64?  Because the MegaRAID card supports only
 * 26 scatter/gather segments.  26 * 4k page == 104k.  The power of 2
 * below that is 64K.  The intention is for per-device discovery of this
 * value in the future.  FIXME.
 */
#define ASM_MAX_IOSIZE	  (1024 * 64)

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
static struct address_space_operations asmfs_aops;
static struct file_operations asmfs_dir_operations;
static struct file_operations asmfs_file_operations;
static struct inode_operations asmfs_dir_inode_operations;

static kmem_cache_t	*asm_request_cachep;

/*
 * osmfs super-block data in memory
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

	/* Remaining extras */
	long max_dentries;
	long free_dentries;
	long max_file_pages;
	long max_pages;
	long free_pages;
};

#define ASMFS_SB(sb) ((struct asmfs_sb_info *)((sb)->u.generic_sbp))


struct asmfs_file_info {
	struct file *f_file;
	spinlock_t f_lock;		/* Lock on the structure */
	wait_queue_head_t f_wait;	/* Folks waiting on I/O */
	struct list_head f_ctx;		/* Hook into the i_threads list */
	struct list_head f_ios;		/* Outstanding I/Os for this thread */
	struct list_head f_complete;	/* Completed I/Os for this thread */
	struct list_head f_disks;	/* List of disks opened */
};

#define ASMFS_FILE(_f) ((struct asmfs_file_info *)((_f)->private_data))


#define ASM_HASH_BUCKETS 1024
/*
 * osmfs inode data in memory
 *
 * Note that 'thread' here can mean 'process' too :-)
 */
struct asmfs_inode_info {
	struct inode *i_inode;
	spinlock_t i_lock;		/* lock on the asmfs_inode_info structure */
	struct list_head disk_hash[ASM_HASH_BUCKETS]; /* Hash of disk handles */
	struct list_head i_threads;	/* list of context structures for each calling thread */
};

#define ASMFS_INODE(_i) ((struct asmfs_inode_info *)((_i->u.generic_ip)))



/*
 * osm disk info
 */
struct asm_disk_info {
	struct asmfs_inode_info *d_inode;
	struct list_head d_hash;
	kdev_t d_dev;
	atomic_t d_count;
	atomic_t d_ios;			/* Count of in-flight I/Os */
	struct list_head d_open;	/* List of assocated asm_disk_heads */
};


/*
 * osm disk info lists
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
	kvec_cb_t r_cb;				/* kvec info */
	/* r_buffers buffers point to user memory */
	/* r_bh buffers is the list with bounce buffers */
	struct buffer_head *r_buffers;		/* Mapped buffers */
	struct buffer_head *r_bh;		/* I/O buffers */
	struct buffer_head *r_bhtail;		/* tail of buffers */
	unsigned long r_bh_count;		/* Number of buffers */
	atomic_t r_io_count;			/* Atomic count */
};


#ifdef CONFIG_UNITEDLINUX_KERNEL
# if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,19)
#  include <linux/kernel_stat.h>
void submit_bh_blknr(int rw, struct buffer_head * bh)
{
	int count = bh->b_size >> 9;

	if (!test_bit(BH_Lock, &bh->b_state))
		BUG();

	if (buffer_delay(bh) || !buffer_mapped(bh))
		BUG();

	set_bit(BH_Req, &bh->b_state);

	/*
	 * First step, 'identity mapping' - RAID or LVM might
	 * further remap this.
	 */
	bh->b_rdev = bh->b_dev;
	bh->b_rsector = bh->b_blocknr;

	generic_make_request(rw, bh);

	switch (rw) {
		case WRITE:
			kstat.pgpgout += count;
			break;
		default:
			kstat.pgpgin += count;
			break;
	}
	conditional_schedule();
}
# else
#  error Invalid United Linux kernel
# endif  /* LINUX_VERSION_CODE */
#endif  /* CONFIG_UNITEDLINUX_KERNEL */


/*
 * Validate that someone made this block device happy for ASM
 */
static int asm_validate_disk(kdev_t dev)
{
	struct buffer_head *bh;
	int blocksize, hblock, ret;

	blocksize = BLOCK_SIZE;
	hblock = get_hardsect_size(dev);
	if (blocksize < hblock)
		blocksize = hblock;

	if (!(bh = bread(dev, 0, blocksize)))
		return -ENXIO;

	ret = 0;
	if (memcmp(bh->b_data + ASM_DISK_LABEL_OFFSET, ASM_DISK_LABEL,
		   ASM_DISK_LABEL_SIZE))
		ret = -EPERM;
	
	brelse(bh);

	return ret;
}  /* asm_validate_disk() */


/*
 * Hashing.  Here's the problem.  When node 2 opens a disk, we need to
 * find out if node 1 has opened it already.  If so, we have to get the
 * handle node1 got so that the handles are identical.  Now, in 2.4 with
 * 128 disks, a list of current disk handles is easy to scan.  As 2.5
 * comes forward with thousands of disks, it doesn't get so hot.  The
 * only semi-sure way to know a disk is the same is via the device
 * major and minor.  Hence, a lookup on major/minor.  However, that's a
 * lookup array of 64K items in 2.4 and 4GB in 2.5.  You could try to
 * only map certain majors, but given that a 20bit minor in 2.5 has 64K
 * disks, it still doesn't work.  So we hash.  Figuring a system with
 * 16K disks (a lot, at least for now), a hash that drops things into
 * 1024 buckets creates collision lists that are only 64 entries.  Even
 * in somewhat bad cases we don't get many more entries per list.
 *
 * All that means that we're going to use a simple hash based on that
 * 1024 bucket table.  The hash?  kdev_t >> 4 & 0x3FF (last 14 bits,
 * dropping four).  This is intended to see SCSI disks, ignoring
 * partition minors.  Come up with a better one.  Come up with a better
 * search.  This isn't set in stone, it's just a start.
 * FIXME: Since we can, in theory, have non-SCSI disks, this needs
 * imporvement.
 */
#define ASM_HASH_DISK(dev) (((dev)>>4) & 0x3FF)


static inline struct asm_disk_info *asm_find_disk(struct asmfs_inode_info *aii, kdev_t dev)
{
	struct list_head *l, *p;
	struct asm_disk_info *d = NULL;

	LOG_ENTRY();

	LOG("ASM: Looking up disk %d,%d\n", MAJOR(dev), MINOR(dev));
	
	spin_lock_irq(&aii->i_lock);
	l = &(aii->disk_hash[ASM_HASH_DISK(dev)]);
	if (list_empty(l))
		goto out;

	list_for_each(p, l) {
		d = list_entry(p, struct asm_disk_info, d_hash);
		if (d->d_dev == dev) {
			atomic_inc(&d->d_count);
			break;
		}
		d = NULL;
	}

out:
	spin_unlock_irq(&aii->i_lock);

	LOG_EXIT_RET(d);
	return d;
}  /* asm_find_disk() */


static inline struct asm_disk_info *asm_add_disk(struct asmfs_inode_info *aii, kdev_t dev)
{
	struct list_head *l, *p;
	struct asm_disk_info *d, *n;

	if (!aii)
		BUG();

	/* FIXME: Maybe a kmem_cache_t later */
	n = kmalloc(sizeof(*n), GFP_KERNEL);
	if (!n)
		return NULL;

	d = NULL;
	spin_lock_irq(&aii->i_lock);
	l = &(aii->disk_hash[ASM_HASH_DISK(dev)]);

	if (list_empty(l))
		goto out;

	list_for_each(p, l) {
		d = list_entry(l, struct asm_disk_info, d_hash);
		if (d->d_dev == dev)
			break;
		d = NULL;
	}
out:
	if (!d) {
		n->d_inode = aii;
		n->d_dev = dev;
		atomic_set(&n->d_count, 1);
		INIT_LIST_HEAD(&n->d_open);
		atomic_set(&n->d_ios, 0);

		list_add(&n->d_hash, l);
		d = n;
	}
	else
		kfree(n);
	spin_unlock_irq(&aii->i_lock);

	return d;
}  /* asm_add_disk() */


static inline void asm_put_disk(struct asm_disk_info *d)
{
	if (!d)
		BUG();

	if (atomic_dec_and_test(&d->d_count)) {
		dprintk("ASM: Freeing disk 0x%.8lX\n", (unsigned long)d->d_dev);
		if (!list_empty(&d->d_open))
			BUG();

		kfree(d);
	}
}  /* asm_put_disk() */


#ifdef DEBUG_BROKEN
/* Debugging code */
static void osmdump_file(struct asmfs_file_info *afi)
{
	struct list_head *p;
	struct asm_disk_head *h;
	struct asm_disk_info *d;
	struct asm_request *r;

	dprintk("ASM: Dumping asmfs_file_info 0x%p\n", afi);
	dprintk("ASM: Opened disks:\n");
	list_for_each(p, &afi->f_disks) {
		h = list_entry(p, struct asm_disk_head, h_flist);
		d = h->h_disk;
		dprintk("ASM: 	0x%p [0x%02X, 0x%02X]\n",
		       d, MAJOR(d->d_dev), MINOR(d->d_dev));
	}
	spin_lock_irq(&afi->f_lock);
	dprintk("ASM: Pending I/Os:\n");
	list_for_each(p, &afi->f_ios) {
		r = list_entry(p, struct asm_request, r_list);
		d = r->r_disk;
		dprintk("ASM:	0x%p (against [0x%02X, 0x%02X])\n",
		       r, MAJOR(d->d_dev), MINOR(d->d_dev));
	}

	dprintk("ASM: Complete I/Os:\n");
	list_for_each(p, &afi->f_complete) {
		r = list_entry(p, struct asm_request, r_list);
		dprintk("ASM:	0x%p\n", r);
	}

	spin_unlock_irq(&afi->f_lock);
}  /* osmdump_file() */

static void osmdump_inode(struct asmfs_inode_info *aii)
{
	int i;
	struct list_head *p, *q, *hash;
	struct asm_disk_info *d;
	struct asm_disk_head *h;
	struct asmfs_file_info *f;

	spin_lock_irq(&aii->i_lock);
	dprintk("ASM: Dumping asmfs_inode_info 0x%p\n", aii);
	dprintk("ASM: Open threads:\n");
	list_for_each(p, &aii->i_threads) {
		f = list_entry(p, struct asmfs_file_info, f_ctx);
		dprintk("ASM:	0x%p\n", f);
	}

	dprintk("ASM: Known disks:\n");
	for (i = 0; i < ASM_HASH_BUCKETS; i++) {
		hash = &(aii->disk_hash[i]);
		if (list_empty(hash))
			continue;
		list_for_each(p, hash) {
			d = list_entry(p, struct asm_disk_info, d_hash);
			dprintk("ASM: 	0x%p, [0x%02X, 0x%02X]\n",
			       d, MAJOR(d->d_dev), MINOR(d->d_dev));
			dprintk("ASM:	Owners:\n");
			list_for_each(q, &d->d_open) {
				h = list_entry(q, struct asm_disk_head,
					       h_dlist);
				f = h->h_file;
				dprintk("ASM:		0x%p\n", f);
			}
		}
	}
	spin_unlock_irq(&aii->i_lock);
}  /* osmdump_inode() */
#endif  /* DEBUG_BROKEN */



/*
 * Resource limit helper functions
 */

static inline void lock_asb(struct asmfs_sb_info *asb)
{
	spin_lock_irq(&(asb->asmfs_lock));
}

static inline void unlock_asb(struct asmfs_sb_info *asb)
{
	spin_unlock_irq(&(asb->asmfs_lock));
}

/* Decrements the free inode count and returns true, or returns false
 * if there are no free inodes */
static int asmfs_alloc_inode(struct super_block *sb)
{
	struct asmfs_sb_info *asb = ASMFS_SB(sb);
	int ret = 1;

	lock_asb(asb);
	if (!asb->max_inodes || asb->free_inodes > 0)
		asb->free_inodes--;
	else
		ret = 0;
	unlock_asb(asb);
	
	return ret;
}

/* Increments the free inode count */
static void asmfs_dealloc_inode(struct super_block *sb)
{
	struct asmfs_sb_info *asb = ASMFS_SB(sb);
	
	lock_asb(asb);
	asb->free_inodes++;
	unlock_asb(asb);
}


/* If the given page can be added to the give inode for osmfs, return
 * true and update the filesystem's free page count and the inode's
 * i_blocks field. Always returns true if the file is already used by
 * osmfs (ie. PageDirty(page) is true)  */
int asmfs_alloc_page(struct inode *inode, struct page *page)
{
	struct asmfs_sb_info *asb = ASMFS_SB(inode->i_sb);
	int ret = 1;

	lock_asb(asb);
		
	if ( (asb->free_pages > 0) &&
	     ( !asb->max_file_pages ||
	       (inode->i_data.nrpages <= asb->max_file_pages) ) ) {
		inode->i_blocks += IBLOCKS_PER_PAGE;
		asb->free_pages--;
	} else
		ret = 0;
	
	unlock_asb(asb);

	return ret;
}

void asmfs_dealloc_page(struct inode *inode, struct page *page)
{
	struct asmfs_sb_info *asb = ASMFS_SB(inode->i_sb);

	if (! Page_Uptodate(page))
		return;

	lock_asb(asb);

	ClearPageDirty(page);
	
	asb->free_pages++;
	inode->i_blocks -= IBLOCKS_PER_PAGE;
	
	if (asb->free_pages > asb->max_pages) {
		dprintk(KERN_ERR "ASM: Error in page allocation, free_pages (%ld) > max_pages (%ld)\n", asb->free_pages, asb->max_pages);
	}

	unlock_asb(asb);
}



static int asmfs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = ASMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_namelen = 255;

	return 0;
}

/*
 * Lookup the data. This is trivial - if the dentry didn't already
 * exist, we know it is negative.
 */
static struct dentry * asmfs_lookup(struct inode *dir, struct dentry *dentry)
{
	d_add(dentry, NULL);
	return NULL;
}

struct inode *asmfs_get_inode(struct super_block *sb, int mode, int dev)
{
	int i;
	struct inode * inode;
	struct asmfs_inode_info *aii;

	if (! asmfs_alloc_inode(sb))
		return NULL;

	inode = new_inode(sb);

	if (inode) {
		aii = kmalloc(sizeof(struct asmfs_inode_info),
			     GFP_KERNEL);
		aii->i_inode = inode;

		for (i = 0; i < ASM_HASH_BUCKETS; i++)
			INIT_LIST_HEAD(&(aii->disk_hash[i]));

		INIT_LIST_HEAD(&aii->i_threads);
		spin_lock_init(&aii->i_lock);
		ASMFS_INODE(inode) = aii;
		if (!aii)
		{
			asmfs_dealloc_inode(sb);
			kfree(inode);
			return NULL;
		}

		inode->i_mode = mode;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_rdev = NODEV;
		inode->i_mapping->a_ops = &asmfs_aops;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_fop = &asmfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &asmfs_dir_inode_operations;
			inode->i_fop = &asmfs_dir_operations;
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
		}
	} else
		asmfs_dealloc_inode(sb);

	return inode;
}

/*
 * File creation. Allocate an inode, update free inode and dentry counts
 * and we're done..
 */
static int asmfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev)
{
	struct inode * inode;
	int error = -ENOSPC;

	dprintk("ASM: Alloc'd dentry\n");
	inode = asmfs_get_inode(dir->i_sb, mode, dev);

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);		/* Extra count - pin the dentry in core */
		error = 0;
	}

	return error;
}

static int asmfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
	return asmfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static inline int asmfs_positive(struct dentry *dentry)
{
	return dentry->d_inode && !d_unhashed(dentry);
}

/*
 * Check that a directory is empty (this works
 * for regular files too, they'll just always be
 * considered empty..).
 *
 * Note that an empty directory can still have
 * children, they just all have to be negative..
 */
static int asmfs_empty(struct dentry *dentry)
{
	struct list_head *list;

	spin_lock_irq(&dcache_lock);
	list = dentry->d_subdirs.next;

	while (list != &dentry->d_subdirs) {
		struct dentry *de = list_entry(list, struct dentry, d_child);

		if (asmfs_positive(de)) {
			spin_unlock_irq(&dcache_lock);
			return 0;
		}
		list = list->next;
	}
	spin_unlock_irq(&dcache_lock);
	return 1;
}

/*
 * This works for both directories and regular files.
 * (non-directories will always have empty subdirs)
 */
static int asmfs_unlink(struct inode * dir, struct dentry *dentry)
{
	int retval = -ENOTEMPTY;

	if (asmfs_empty(dentry)) {
		struct inode *inode = dentry->d_inode;

		inode->i_nlink--;
		dput(dentry);			/* Undo the count from "create" - this does all the work */

		retval = 0;
	}
	return retval;
}

static void asmfs_delete_inode(struct inode *inode)
{
	asmfs_dealloc_inode(inode->i_sb);

	clear_inode(inode);
}

static void asmfs_put_super(struct super_block *sb)
{
	kfree(sb->u.generic_sbp);
}

struct asmfs_params {
	uid_t uid;
	gid_t gid;
	int mode;
	long inodes;
};

static int parse_options(char * options, struct asmfs_params *p)
{
	char save = 0, *savep = NULL, *optname, *value;

	p->inodes = -1;
	p->uid = current->fsuid;
	p->gid = current->fsgid;
	p->mode = 0;

	for (optname = strtok(options,","); optname;
	     optname = strtok(NULL,",")) {
		if ((value = strchr(optname,'=')) != NULL) {
			save = *value;
			savep = value;
			*value++ = 0;
		}

		if (!strcmp(optname, "uid") && value) {
			p->uid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		} else if (!strcmp(optname, "gid") && value) {
			p->gid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		} else if (!strcmp(optname, "mode") && value) {
			p->mode = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		} else if (!strcmp(optname, "maxinstances") && value) {
			p->inodes = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}

		if (optname != options)
			*(optname-1) = ',';
		if (value)
			*savep = save;
/*  		if (ret == 0) */
/*  			break; */
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
	lock_asb(asb);

	if (p->inodes >= 0) {
		int used_inodes = asb->max_inodes - asb->free_inodes;

		asb->max_inodes = p->inodes;
		asb->free_inodes = asb->max_inodes - used_inodes;
	}

	unlock_asb(asb);
}

static int asmfs_remount(struct super_block * sb, int * flags, char * data)
{
	struct asmfs_params params;
	struct asmfs_sb_info * asb = ASMFS_SB(sb);

	if (parse_options((char *)data, &params) != 0)
		return -EINVAL;

	reset_limits(asb, &params);

	printk(KERN_DEBUG "ASM: osmfs remounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "ASM:	maxinstances=%ld\n",
	       asb->max_inodes);

	return 0;
}

static int asm_disk_open(struct asmfs_file_info *afi, struct asmfs_inode_info *aii, kdev_t dev)
{
	struct asm_disk_info *d;
	struct asm_disk_head *h;

	if (!afi || !aii)
		BUG();

	h = kmalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return 0;

	d = asm_find_disk(aii, dev);
	if (!d) {
		/* FIXME: Need to verify it's a valid device here */
		d = asm_add_disk(aii, dev);
	}

	h->h_disk = d;
	h->h_file = afi;

	spin_lock_irq(&afi->f_lock);
	list_add(&h->h_flist, &afi->f_disks);
	spin_unlock_irq(&afi->f_lock);

	spin_lock_irq(&aii->i_lock);
	list_add(&h->h_dlist, &d->d_open);
	spin_unlock_irq(&aii->i_lock);

	return !d;
}  /* asm_disk_open() */


static int asm_disk_close(struct asmfs_file_info *afi, struct asmfs_inode_info *aii, kdev_t dev)
{
	struct asm_disk_info *d;
	struct list_head *p;
	struct asm_disk_head *h;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	if (!afi || !aii)
		BUG();

	d = asm_find_disk(aii, dev);
	if (!d)
		return -EINVAL;

	spin_lock_irq(&afi->f_lock);
	h = NULL;
	list_for_each(p, &afi->f_disks) {
		h = list_entry(p, struct asm_disk_head, h_flist);
		if (h->h_disk == d)
			break;
		h = NULL;
	}
	if (!h)
		BUG();

	list_del(&h->h_flist);
	spin_unlock_irq(&afi->f_lock);

	spin_lock_irq(&aii->i_lock);
	list_del(&h->h_dlist);

	/* Last close */
	if (list_empty(&d->d_open))
	{
		dprintk("Last close of disk 0x%.8lX (0x%p, pid=%d)\n",
		       (unsigned long)d->d_dev, d, tsk->pid);
		list_del(&d->d_hash);

		spin_unlock_irq(&aii->i_lock);

		/* No need for a fast path */
		add_wait_queue(&afi->f_wait, &wait);
		do {
			run_task_queue(&tq_disk);
			set_task_state(tsk, TASK_UNINTERRUPTIBLE);

			if (!atomic_read(&d->d_ios))
				break;

			/*
			 * Timeout of one second.  This is slightly
			 * subtle.  In this wait, and *only* this wait,
			 * we are waiting on I/Os that might have been
			 * initiated by another process.  In that case,
			 * the other process's afi will be signaled,
			 * not ours, so the wake_up() never happens
			 * here and we need the timeout.
			 */
			schedule_timeout(HZ);
		} while (1);
		set_task_state(tsk, TASK_RUNNING);
		remove_wait_queue(&afi->f_wait, &wait);
	}
	else
		spin_unlock_irq(&aii->i_lock);

	kfree(h);

	/* Drop the ref from asm_find_disk() */
	asm_put_disk(d);

	/* Real put */
	asm_put_disk(d);

	return 0;
}  /* asm_disk_close() */


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


static int asm_update_user_ioc(struct asm_request *r)
{
	asm_ioc *ioc;
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
		r->r_status = 0;
		r->r_error = 0;
		r->r_buffers = NULL;
		r->r_bh = NULL;
		r->r_bhtail = NULL;
		r->r_elapsed = 0;
		r->r_cb.vec = NULL;
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
	struct asm_disk_info *d = r->r_disk;
	struct asmfs_file_info *afi = r->r_file;
	unsigned long flags;

	if (!afi)
		BUG();

	if (d) {
		r->r_disk = NULL;
		atomic_dec(&d->d_ios);
		if (atomic_read(&d->d_ios) < 0) {
			printk("ASM: d_ios underflow!\n");
			atomic_set(&d->d_ios, 0);
		}
		asm_put_disk(d);
	}

	spin_lock_irqsave(&afi->f_lock, flags);
	list_del(&r->r_list);
	list_add(&r->r_list, &afi->f_complete);
	if (r->r_error)
		r->r_status |= ASM_ERROR;
	else
		r->r_status |= ASM_COMPLETED;

	spin_unlock_irqrestore(&afi->f_lock, flags);

	r->r_elapsed = ((jiffies - r->r_elapsed) * 1000000) / HZ;

	wake_up(&afi->f_wait);
}  /* asm_finish_io() */


static void asm_end_kvec_io(void *_req, struct kvec *vec, ssize_t res)
{
	struct asm_request *r = _req;
	ssize_t err = 0;
	
	LOG_ENTRY();
	if (!r)
		BUG();

	LOG("ASM: Releasing kvec for request at 0x%p\n", r);

	if (res < 0) {
		err = res;
		res = 0;
	}

	while (r->r_buffers) {
		struct buffer_head *tmp = r->r_buffers;
		r->r_buffers = tmp->b_next;
		if (!err) {
		       	if (buffer_uptodate(tmp))
				res += tmp->b_size;
			else
				err = -EIO;
		}
		kmem_cache_free(bh_cachep, tmp);
	}
	r->r_bh = r->r_bhtail = NULL;

	if (vec) {
		unmap_kvec(vec, 0);
		free_kvec(vec);
		r->r_cb.vec = NULL;
	}

	switch (err) {
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
			break;

		case -EINVAL:
			r->r_error = ASM_ERR_INVAL;
			break;
	}

	asm_finish_io(r);

	LOG_EXIT();
}  /* asm_end_kvec_io() */


static void asm_end_buffer_io(struct buffer_head *bh, int uptodate)
{
	struct asm_request *r;

	LOG_ENTRY();
	mark_buffer_uptodate(bh, uptodate);

	r = bh->b_private;
	LOG("ASM: Releasing buffer for request at 0x%p\n", r);
	unlock_buffer(bh);

	if (atomic_dec_and_test(&r->r_io_count)) {
		asm_end_kvec_io(r, r->r_cb.vec, 0);
	}

	LOG_EXIT();
}  /* asm_end_buffer_io() */


static int asm_build_io(int rw, struct asm_request *r,
			unsigned long blknr, unsigned long nr)
{
	struct kvec	*vec = r->r_cb.vec;
	struct kveclet	*veclet;
	int		err;
	int		length;
	kdev_t		dev = r->r_disk->d_dev;
	unsigned	sector_size = get_hardsect_size(dev);
	int		i;
#ifdef CONFIG_UNITEDLINUX_KERNEL
# if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,19)
	unsigned int    atomic_seq;
# else
#  error Invalid United Linux kernel
# endif  /* LINUX_VERSION_CODE */
#endif  /* CONFIG_UNITEDLINUX_KERNEL */

	LOG_ENTRY();
	if (!vec->nr)
		BUG();

	/* 
	 * First, do some alignment and validity checks 
	 */
	length = 0;
	for (veclet=vec->veclet, i=0; i < vec->nr; i++,veclet++) {
		length += veclet->length;
		if ((veclet->offset & (sector_size-1)) ||
		    (veclet->length & (sector_size-1)) ||
		    ((veclet->length + veclet->offset) > PAGE_SIZE)) {
			dprintk("ASM: asm_build_io: tuple[%d]->offset=0x%x length=0x%x sector_size: 0x%x\n", i, veclet->offset, veclet->length, sector_size);
			return -EINVAL;
		}
	}

	/* We deal in blocks, so this should be valid */
	if (length != (nr * sector_size))
		BUG();

	/* 
	 * OK to walk down the iovec doing page IO on each page we find. 
	 */
	err = 0;

	if (!nr) {
		dprintk("ASM: asm_build_io: !i\n");
		return -EINVAL;
	}

#ifdef CONFIG_UNITEDLINUX_KERNEL
# if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,19)
	atomic_seq = blk_get_atomic_seq();
# else
#  error Invalid United Linux kernel
# endif  /* LINUX_VERSION_CODE */
#endif  /* CONFIG_UNITEDLINUX_KERNEL */
	
	r->r_bh_count = 0;

	/* This is massaged from fs/buffer.c.  Blame Ben. */
	/* This is ugly.  FIXME. */
	for (i=0, veclet=vec->veclet; i<vec->nr; i++,veclet++) {
		struct page *page = veclet->page;
		unsigned offset = veclet->offset;
		unsigned iosize = veclet->length;
		struct buffer_head *tmp;
	       
		if (!page)
			BUG();

		tmp = kmem_cache_alloc(bh_cachep, GFP_NOIO);
		err = -ENOMEM;
		if (!tmp)
			goto error;

		tmp->b_dev = B_FREE;
		tmp->b_size = iosize;
		set_bh_page(tmp, page, offset);
		tmp->b_this_page = tmp;

		init_buffer(tmp, asm_end_buffer_io, NULL);

		tmp->b_dev = dev;
		tmp->b_blocknr = tmp->b_rsector = blknr;

		blknr += (iosize / sector_size);
		tmp->b_state = (1 << BH_Mapped) | (1 << BH_Lock)
				| (1 << BH_Req);
		tmp->b_private = r;
		tmp->b_next = tmp->b_reqnext = NULL;
		atomic_set(&tmp->b_count, 1);

#ifdef CONFIG_UNITEDLINUX_KERNEL
# if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,19)
		bh_elv_seq(tmp) = atomic_seq;
# else
#  error Invalid United Linux kernel
# endif  /* LINUX_VERSION_CODE */
#endif  /* CONFIG_UNITEDLINUX_KERNEL */

		if (rw == WRITE) {
			set_bit(BH_Uptodate, &tmp->b_state);
			clear_bit(BH_Dirty, &tmp->b_state);
		}

#if 0 /* delete later */
		/* Make tmp a bounce buffer if needed */
		iobh = blk_queue_bounce(q, rw, tmp);
		if (iobh != tmp) {
			iobh->b_reqnext = NULL;
			init_waitqueue_head(&iobh->b_wait);
		}

		if (r->r_bh) {
			if (!BH_CONTIG(r->r_bhtail, iobh) ||
			    !BH_PHYS_4G(r->r_bhtail, iobh)) {
				r->r_seg_count++;
			}

			r->r_bhtail->b_reqnext = iobh;
			r->r_bhtail = iobh;
		} else {
			r->r_bh = r->r_bhtail = iobh;
			r->r_seg_count = 1;
		}
		/* Keep a list of original buffers */
		tmp->b_next = r->r_buffers;
#else

		if (r->r_bh)
		{
			r->r_bhtail->b_next = tmp;
#ifdef RED_HAT_LINUX_KERNEL
# if (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,9)) || (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,20)) || (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,21))
			r->r_bhtail->b_reqnext = tmp;
# else
#  error Invalid Red Hat Linux kernel
# endif  /* LINUX_VERSION_CODE */
#endif  /* RED_HAT_LINUX_KERNEL */
     		}
		else
			r->r_bh = tmp;

		r->r_bhtail = tmp;
#endif /* 0 */
		r->r_bh_count++;
	} /* End of page loop */		

	r->r_buffers = r->r_bh;

	atomic_set(&r->r_io_count, r->r_bh_count);

	LOG_EXIT_RET(0);
	return 0;

error:
	/* Walk bh list, freeing them */
	while (r->r_bh) {
		struct buffer_head *tmp = r->r_bh;
		r->r_bh = tmp->b_next;
		kmem_cache_free(bh_cachep, tmp);
	}
	r->r_bhtail = NULL;

	LOG_EXIT_RET(err);
	return err;
}  /* asm_build_io() */


static int asm_submit_io(struct asmfs_file_info *afi, 
			 struct asmfs_inode_info *aii,
			 asm_ioc *user_iocp,
			 asm_ioc *ioc)
{
	int ret, major, rw = READ, minorsize = 0;
	struct asm_request *r;
	struct asm_disk_info *d;
	size_t count;
	struct buffer_head *bh;
	kdev_t kdv;

	LOG_ENTRY();
	if (!ioc)
		return -EINVAL;

	if (ioc->status_asm_ioc)
		return -EINVAL;

	r = asm_request_alloc();
	if (!r)
		return -ENOMEM;
	LOG("ASM: New request at 0x%p alloc()ed for user ioc at 0x%p\n", r, user_iocp);

	r->r_file = afi;
	r->r_ioc = user_iocp;  /* Userspace asm_ioc */

	spin_lock_irq(&afi->f_lock);
	list_add(&r->r_list, &afi->f_ios);
	spin_unlock_irq(&afi->f_lock);

	ret = -ENODEV;
	kdv = to_kdev_t(ioc->disk_asm_ioc);
	d = asm_find_disk(aii, kdv);
	if (!d)
		goto out_error;

	atomic_inc(&d->d_ios);
	r->r_disk = d;

	count = ioc->rcount_asm_ioc * get_hardsect_size(d->d_dev);

	LOG("ASM: first, 0x%08lX.%08lX; masked 0x%08lX\n",
	    HIGH_UB4(ioc->first_asm_ioc), LOW_UB4(ioc->first_asm_ioc),
	       (unsigned long)ioc->first_asm_ioc);
	/* linux only supports unsigned long size sector numbers */
	LOG("ASM: status: %u, buffer_asm_ioc: 0x%08lX, count: %u\n",
	    ioc->status_asm_ioc,
	    (unsigned long)ioc->buffer_asm_ioc, count);
	/* Note that priority is ignored for now */
	ret = -EINVAL;
	if (!ioc->buffer_asm_ioc ||
	    (ioc->buffer_asm_ioc != (unsigned long)ioc->buffer_asm_ioc) ||
	    (ioc->first_asm_ioc != (unsigned long)ioc->first_asm_ioc) ||
	    (ioc->rcount_asm_ioc != (unsigned long)ioc->rcount_asm_ioc) ||
	    (ioc->priority_asm_ioc > 7) ||
	    (count > ASM_MAX_IOSIZE) ||
	    (count < 0))
		goto out_error;

	/* Test device size, when known. (massaged from ll_rw_blk.c) */
	major = MAJOR(d->d_dev);
	if (blk_size[major])
		minorsize = blk_size[major][MINOR(d->d_dev)];
	if (minorsize) {
		unsigned long maxsector = (minorsize << 1) + 1;
		unsigned long sector = (unsigned long)ioc->first_asm_ioc;
		unsigned long blks = (unsigned long)ioc->rcount_asm_ioc;

		if (maxsector < blks || maxsector - blks < sector) {
			printk(KERN_INFO
			       "ASM: attempt to access beyond end of device\n");
			printk(KERN_INFO "ASM: dev [0x%03X, 0x%03X]: want=%lu, limit=%u\n",
			       MAJOR(d->d_dev), MINOR(d->d_dev),
			       (sector + blks) >> 1, minorsize);
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

	r->r_cb.vec =
		map_user_kvec(rw, (unsigned long)ioc->buffer_asm_ioc,
			      count);
	r->r_cb.data = r;
	r->r_cb.fn = asm_end_kvec_io;
	if (IS_ERR(r->r_cb.vec)) {
		ret = PTR_ERR(r->r_cb.vec);
		r->r_cb.vec = NULL;
		goto out_error;
	}

	ret = asm_build_io(rw, r,
			   ioc->first_asm_ioc, ioc->rcount_asm_ioc);
	LOG("ASM: Return from asm_build_io() is %d\n", ret);
	if (ret)
		goto out_error;

	r->r_elapsed = jiffies;  /* Set start time */

	bh = r->r_bh;
#ifdef RED_HAT_LINUX_KERNEL
# if (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,9)) || (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,20)) || (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,21))
	submit_bh_linked(rw, bh);
# else
#  error Invalid Red Hat Linux kernel
# endif  /* LINUX_VERSION_CODE */
#else
# ifdef CONFIG_UNITEDLINUX_KERNEL
#  if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,19)
	while (bh)
	{
	    submit_bh_blknr(rw, bh);
	    bh = bh->b_next;
	}
	blk_refile_atomic_queue(bh_elv_seq(r->r_bh));
#  else
#   error Invalid United Linux kernel
#  endif  /* LINUX_VERSION_CODE */
# else
#  error Invalid kernel
# endif  /* CONFIG_UNITEDLINUX_KERNEL */
#endif  /* RED_HAT_LINUX_KERNEL */

	r->r_status |= ASM_SUBMITTED;

out:
	ret = asm_update_user_ioc(r);

	LOG_EXIT_RET(ret);
	return ret;

out_error:
	asm_end_kvec_io(r, r->r_cb.vec, ret);
	goto out;
}  /* asm_submit_io() */


static int asm_maybe_wait_io(struct asmfs_file_info *afi, 
			     struct asmfs_inode_info *aii,
			     asm_ioc *iocp,
			     struct timeout *to)
{
	long ret;
	u64 p;
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
	    list_empty(&r->r_list)) {
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
			ret = 0;
			run_task_queue(&tq_disk);
			set_task_state(tsk, TASK_INTERRUPTIBLE);

			spin_lock_irq(&afi->f_lock);
			if (r->r_status & (ASM_COMPLETED |
					   ASM_BUSY | ASM_ERROR))
				break;
			spin_unlock_irq(&afi->f_lock);

			ret = -ETIMEDOUT;
			if (to->timed_out)
				break;
			schedule();
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

	/* Somebody got here first */
	if (r->r_status & ASM_FREE)
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

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io() */


static int asm_complete_io(struct asmfs_file_info *afi, 
			   struct asmfs_inode_info *aii,
			   asm_ioc **ioc)
{
	int ret = 0;
	struct list_head *l;
	struct asm_request *r;

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


static int asm_wait_completion(struct asmfs_file_info *afi,
			       struct asmfs_inode_info *aii,
			       struct oracleasm_io *io,
			       struct timeout *to,
			       u32 *status)
{
	int ret;
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
		ret = 0;
		run_task_queue(&tq_disk);
		set_task_state(tsk, TASK_INTERRUPTIBLE);

		spin_lock_irq(&afi->f_lock);
		if (!list_empty(&afi->f_complete)) {
			spin_unlock_irq(&afi->f_lock);
			break;
		}
		spin_unlock_irq(&afi->f_lock);

		ret = -ETIMEDOUT;
		if (to->timed_out)
			break;
		schedule();
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


static inline int asm_submit_io_native(struct asmfs_file_info *afi, 
	       			       struct asmfs_inode_info *aii,
       				       struct oracleasm_io *io)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;
	asm_ioc tmp;

	LOG_ENTRY();
	for (i = 0; i < io->io_reqlen; i++) {
		ret = -EFAULT;
		if (get_user(iocp,
			     ((asm_ioc **)((unsigned long)io->io_requests)) + i))
			break;

		if (copy_from_user(&tmp, iocp, sizeof(tmp)))
			break;

		LOG("ASM: Submitting user ioc at 0x%p\n", iocp);
		ret = asm_submit_io(afi, aii, iocp, &tmp);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_submit_io_native() */


static inline int asm_maybe_wait_io_native(struct asmfs_file_info *afi, 
					   struct asmfs_inode_info *aii,
					   struct oracleasm_io *io,
					   struct timeout *to)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_waitlen; i++) {
		if (get_user(iocp,
			     ((asm_ioc **)((unsigned long)io->io_waitreqs)) + i))
			return -EFAULT;

		ret = asm_maybe_wait_io(afi, aii, iocp, to);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io_native() */


static inline int asm_complete_ios_native(struct asmfs_file_info *afi,
					  struct asmfs_inode_info *aii,
					  struct oracleasm_io *io,
					  struct timeout *to,
					  u32 *status)
{
	int ret = 0;
	u32 i;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_complen; i++) {
		ret = asm_complete_io(afi, aii, &iocp);
		if (ret)
			break;
		if (iocp) {
			ret = put_user(iocp,
				       ((asm_ioc **)((unsigned long)io->io_completions)) + i);
			       if (ret)
				   break;
			continue;
		}

		/* We had waiters that are full */
		if (*status & ASM_IO_WAITED)
			break;

		ret = asm_wait_completion(afi, aii, io, to, status);
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
	LOG("(0x%lX, 0x%lX)\n",
	    ioc->check_asm_ioc,
	    ioc->buffer_asm_ioc);

	LOG_EXIT();
}  /* asm_promote_64() */


static inline int asm_submit_io_thunk(struct asmfs_file_info *afi, 
	       			      struct asmfs_inode_info *aii,
	       			      struct oracleasm_io *io)
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
			     ((u32 *)((unsigned long)io->io_requests)) + i))
			break;

		iocp = (asm_ioc32 *)(unsigned long)iocp_32;

		if (copy_from_user(&tmp, iocp, sizeof(*iocp)))
			break;

		asm_promote_64(&tmp);

		ret = asm_submit_io(afi, aii, (asm_ioc *)iocp, &tmp);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_submit_io_thunk() */


static inline int asm_maybe_wait_io_thunk(struct asmfs_file_info *afi, 
					  struct asmfs_inode_info *aii,
					  struct oracleasm_io *io,
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
			     ((u32 *)((unsigned long)io->io_waitreqs)) + i))
			return -EFAULT;

		/* Remember, the this is pointing to 32bit userspace */
		iocp = (asm_ioc *)(unsigned long)iocp_32;

		ret = asm_maybe_wait_io(afi, aii, iocp, to);
		if (ret)
			break;
	}

	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_maybe_wait_io_thunk() */


static inline int asm_complete_ios_thunk(struct asmfs_file_info *afi,
					 struct asmfs_inode_info *aii,
					 struct oracleasm_io *io,
					 struct timeout *to,
					 u32 *status)
{
	int ret = 0;
	u32 i;
	u32 iocp_32;
	asm_ioc *iocp;

	LOG_ENTRY();
	for (i = 0; i < io->io_complen; i++) {
		ret = asm_complete_io(afi, aii, &iocp);
		if (ret)
			break;
		if (iocp) {
			iocp_32 = (u32)(unsigned long)iocp;

			ret = put_user(iocp_32,
				       ((u32 *)((unsigned long)io->io_completions)) + i);
			       if (ret)
				   break;
			continue;
		}

		/* We had waiters that are full */
		if (*status & ASM_IO_WAITED)
			break;

		ret = asm_wait_completion(afi, aii, io, to, status);
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


static int asm_do_io(struct asmfs_file_info *afi,
		     struct asmfs_inode_info *aii,
		     struct oracleasm_io *io, int bpl)
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
				   (struct timespec *)(unsigned long)io->io_timeout,
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
			ret = asm_submit_io_32(afi, aii, io);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_submit_io_64(afi, aii, io);
#endif  /* BITS_PER_LONG == 64 */

		if (ret)
			goto out_to;
	}

	if (io->io_waitreqs) {
		LOG("ASM: oracleasm_io has waits; waitlen %d\n",
		    io->io_waitlen);
		ret = -EINVAL;
		if (bpl == ASM_BPL_32)
			ret = asm_maybe_wait_io_32(afi, aii, io, &to);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_maybe_wait_io_64(afi, aii, io, &to);
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
			ret = asm_complete_ios_32(afi, aii, io,
						  &to, &status);
#if BITS_PER_LONG == 64
		else if (bpl == ASM_BPL_64)
			ret = asm_complete_ios_64(afi, aii, io,
						  &to, &status);
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
	if (put_user(status, (u32 *)(unsigned long)io->io_statusp))
		ret = -EFAULT;
	LOG_EXIT_RET(ret);
	return ret;
}  /* asm_do_io() */


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
	spin_lock_init(&afi->f_lock);
	INIT_LIST_HEAD(&afi->f_ctx);
	INIT_LIST_HEAD(&afi->f_disks);
	INIT_LIST_HEAD(&afi->f_ios);
	INIT_LIST_HEAD(&afi->f_complete);
	init_waitqueue_head(&afi->f_wait);

	aii = ASMFS_INODE(inode);
	spin_lock_irq(&aii->i_lock);
	list_add(&afi->f_ctx, &aii->i_threads);
	spin_unlock_irq(&aii->i_lock);

	ASMFS_FILE(file) = afi;

	return 0;
}  /* asmfs_file_open() */


static int asmfs_file_release(struct inode * inode, struct file * file)
{
	struct asmfs_inode_info *aii;
	struct asmfs_file_info *afi;
	struct list_head *p, *q;
	struct asm_disk_head *h;
	struct asm_disk_info *d;
	struct asm_request *r;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);

	aii = ASMFS_INODE(inode);
	afi = ASMFS_FILE(file);
	ASMFS_FILE(file) = NULL;

	if (!afi)
		BUG();

	/*
	 * Shouldn't need the lock, no one else has a reference
	 * asm_disk_close will need to take it when completing I/O
	 */
	list_for_each_safe(p, q, &afi->f_disks) {
		h = list_entry(p, struct asm_disk_head, h_flist);
		d = h->h_disk;
		asm_disk_close(afi, aii, d->d_dev);
	}

	/* FIXME: Clean up things that hang off of afi */

	spin_lock_irq(&aii->i_lock);
	list_del(&afi->f_ctx);
	spin_unlock_irq(&aii->i_lock);

	/* No need for a fastpath */
	add_wait_queue(&afi->f_wait, &wait);
	do {
		run_task_queue(&tq_disk);
		set_task_state(tsk, TASK_UNINTERRUPTIBLE);

		spin_lock_irq(&afi->f_lock);
		if (list_empty(&afi->f_ios))
		    break;
		spin_unlock_irq(&afi->f_lock);
		dprintk("There are still I/Os hanging off of afi 0x%p\n",
		       afi);
		schedule();
	} while (1);
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

	dprintk("ASM: Done with afi 0x%p\n", afi);
	kfree(afi);

	return 0;
}  /* asmfs_file_release() */


/*
 * Yes, evil ioctl()s.  seq_file doesn't work here, IMHO.
 */
static int asmfs_file_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	kdev_t kdv;
	struct asmfs_file_info *afi = ASMFS_FILE(file);
	struct asmfs_inode_info *aii = ASMFS_INODE(inode);
	struct oracleasm_io io;
	struct oracleasm_disk_query dq;
	int ret;

	LOG_ENTRY();

	switch (cmd) {
		default:
			LOG("ASM: Invalid ioctl 0x%u\n", cmd);
			return -ENOTTY;
			break;

		case ASMIOC_QUERYDISK:
			LOG("ASM: Operation is ASMIOC_QUERYDISK\n");
			if (copy_from_user(&dq,
                                           (struct oracleasm_disk_query *)arg,
					   sizeof(dq)))
				return -EFAULT;
			kdv = to_kdev_t(dq.dq_rdev);
			LOG("ASM: Checking disk %d,%d\n", MAJOR(kdv),
			    MINOR(kdv));
#if 0
			/* Right now we trust only SCSI ->request_fn */
			if (!SCSI_DISK_MAJOR(MAJOR(kdv)))
				return -EINVAL;
#endif

			ret = asm_validate_disk(kdv);
			if (ret)
				return ret;

			dq.dq_maxio = ASM_MAX_IOSIZE;
			if (copy_to_user((struct oracleasm_disk_query *)arg,
					 &dq, sizeof(dq)))
				return -EFAULT;
			LOG("ASM: Valid disk %d,%d\n", MAJOR(kdv),
			    MINOR(kdv));
			break;

		case ASMIOC_OPENDISK:
			LOG("ASM: Operation is ASMIOC_OPENDISK\n");
			if (copy_from_user(&dq,
                                           (struct oracleasm_disk_query *)arg,
					   sizeof(dq)))
				return -EFAULT;
			kdv = to_kdev_t(dq.dq_rdev);
			LOG("ASM: Opening disk %d,%d\n", MAJOR(kdv),
			    MINOR(kdv));

			ret = asm_validate_disk(kdv);
			if (ret)
				return ret;

			/* Userspace checks a 0UL return */
			if (asm_disk_open(afi, aii, kdv))
			    dq.dq_rdev = 0;
			if (copy_to_user((struct oracleasm_disk_query *)arg,
					 &dq, sizeof(dq)))
			    return -EFAULT;
			LOG("ASM: Opened handle 0x%.8lX\n",
			    (unsigned long)dq.dq_rdev);
			break;

		case ASMIOC_CLOSEDISK:
			LOG("ASM: Operation is ASMIOC_CLOSEDISK\n");
			if (copy_from_user(&dq,
                                           (struct oracleasm_disk_query *)arg,
					   sizeof(dq)))
				return -EFAULT;
			LOG("ASM: Closing handle 0x%.8lX\n", (unsigned long)dq.dq_rdev);
			kdv = to_kdev_t(dq.dq_rdev);
			return asm_disk_close(afi, aii, kdv);
			break;

		case ASMIOC_IODISK32:
			LOG("ASM: Operation is ASMIOC_IODISK32\n");
			if (copy_from_user(&io,
                                           (struct oracleasm_io *)arg,
					   sizeof(io)))
				return -EFAULT;
			return asm_do_io(afi, aii, &io, ASM_BPL_32);
			break;

#if BITS_PER_LONG == 64
		case ASMIOC_IODISK64:
			LOG("ASM: Operation is ASM_IODISK64\n");
			if (copy_from_user(&io,
                                           (struct oracleasm_io *)arg,
					   sizeof(io)))
				return -EFAULT;
			return asm_do_io(afi, aii, &io, ASM_BPL_64);
			break;
#endif  /* BITS_PER_LONG == 64 */

		case ASMIOC_DUMP:
			LOG("ASM: Operation is ASMIOC_DUMP\n");
			/* Dump data */
#ifdef DEBUG_BROKEN
			osmdump_file(afi);
			osmdump_inode(aii);
#endif /* DEBUG_BROKEN */
			break;
	}

	LOG_EXIT_RET(0);
	return 0;
}

static int asmfs_dir_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct oracleasm_get_iid new_iid;
	struct asmfs_sb_info *asb = ASMFS_SB(inode->i_sb);

	switch (cmd) {
		default:
			return -ENOTTY;
			break;

		case ASMIOC_GETIID:
			if (copy_from_user(&new_iid,
					   (struct oracleasm_get_iid *)arg,
					   sizeof(new_iid)))
			    return -EFAULT;
			if (new_iid.gi_version != (u64)ASM_ABI_VERSION)
			    return -EINVAL;
			lock_asb(asb);
			new_iid.gi_iid = (u64)asb->next_iid;
			asb->next_iid++;
			unlock_asb(asb);
			if (copy_to_user((struct oracleasm_get_iid *)arg,
					 &new_iid, sizeof(new_iid)))
			    return -EFAULT;
			break;

		case ASMIOC_CHECKIID:
			if (copy_from_user(&new_iid,
					   (struct oracleasm_get_iid *)arg,
					   sizeof(new_iid)))
			    return -EFAULT;
			if (new_iid.gi_version != (u64)ASM_ABI_VERSION)
			    return -EINVAL;
			lock_asb(asb);
			if (new_iid.gi_iid >= (u64)asb->next_iid)
			    new_iid.gi_iid = (u64)0;
			unlock_asb(asb);
			if (!new_iid.gi_iid)
			{
			    if (copy_to_user((struct oracleasm_get_iid *)arg,
					     &new_iid, sizeof(new_iid)))
				return -EFAULT;
			}
			break;
	}

	return 0;
}

static int asmfs_sync_file(struct file * file, struct dentry *dentry, int datasync)
{
	return 0;
}

static struct address_space_operations asmfs_aops = {
};

static struct file_operations asmfs_file_operations = {
	.open		= asmfs_file_open,
	.release	= asmfs_file_release,
	.ioctl		= asmfs_file_ioctl,
};

/*  See init_asmfs_dir_operations() */
static struct file_operations asmfs_dir_operations = {0, };

static struct inode_operations asmfs_dir_inode_operations = {
	.create		= asmfs_create,
	.lookup		= asmfs_lookup,
	.unlink		= asmfs_unlink,
};

static struct super_operations asmfs_ops = {
	.statfs		= asmfs_statfs,
	.put_inode	= force_delete,
	/* These last three only required for limited maxinstances */
	.delete_inode	= asmfs_delete_inode,
	.put_super	= asmfs_put_super,
	.remount_fs     = asmfs_remount,
};

/*
 * Initialisation
 */

static struct super_block *asmfs_read_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct dentry * root;
	struct asmfs_sb_info * asb;
	struct asmfs_params params;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = ASMFS_MAGIC;
	sb->s_op = &asmfs_ops;
	sb->s_maxbytes = MAX_NON_LFS;	/* Why? */

	asb = kmalloc(sizeof(struct asmfs_sb_info), GFP_KERNEL);
	ASMFS_SB(sb) = asb;
	if (!asb)
		return NULL;

	asb->asmfs_super = sb;
	asb->next_iid = 1;
	spin_lock_init(&asb->asmfs_lock);

	if (parse_options((char *)data, &params) != 0)
		return NULL;

	init_limits(asb, &params);

	inode = asmfs_get_inode(sb, S_IFDIR | 0755, 0);
	if (!inode)
		return NULL;

	/* Set root owner */
	inode->i_uid = params.uid;
	inode->i_gid = params.gid;
	if (params.mode) {
		inode->i_mode = (inode->i_mode & 077000) | params.mode;
	} else {
		inode->i_mode |= 0770; /* user + group write */
		inode->i_mode &= 077770; /* squash world perms */
	}

	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return NULL;
	}
	sb->s_root = root;

	printk(KERN_DEBUG "ASM: oracleasmfs mounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "ASM:	uid=%d gid=%d mode=0%o maxinstances=%ld\n",
	       params.uid, params.gid, params.mode,
	       asb->max_inodes);
	return sb;
}


static void __init init_asmfs_dir_operations(void) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,19)
	asmfs_dir_operations.read	= generic_read_dir;
	asmfs_dir_operations.readdir	= dcache_readdir;
#else
	asmfs_dir_operations		= dcache_dir_ops;
#endif  /* LINUX_VERSION_CODE < 2.4.19 */
	asmfs_dir_operations.fsync	= asmfs_sync_file;
	asmfs_dir_operations.ioctl	= asmfs_dir_ioctl;
};


static DECLARE_FSTYPE(asmfs_fs_type, "oracleasmfs", asmfs_read_super, FS_LITTER);

static int __init init_asmfs_fs(void)
{
	LOG("sizeof asm_ioc32: %u\n", sizeof(asm_ioc32));
	asm_request_cachep =
		kmem_cache_create("asm_request",
				  sizeof(struct asm_request),
				  0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!asm_request_cachep)
		panic("Unable to create asm_request cache\n");

	init_asmfs_dir_operations();
	return register_filesystem(&asmfs_fs_type);
}

static void __exit exit_asmfs_fs(void)
{
	unregister_filesystem(&asmfs_fs_type);
	kmem_cache_destroy(asm_request_cachep);
}

module_init(init_asmfs_fs)
module_exit(exit_asmfs_fs)
MODULE_LICENSE("GPL");
