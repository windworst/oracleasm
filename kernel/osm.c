/*
 * osm - Kernel driver for the Oracle Storage Manager
 *
 * Copyright (C) 2002 Oracle Corporation.  All rights reserved.
 *
 * This driver's filesystem code is based on the ramfs filesystem.
 * Copyright information for the original source appears below.  This
 * file is released under the GNU General Public License.
 */

/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
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

#include "arch/osmkernel.h"
#include "osmprivate.h"
#include "osmerror.h"

#if defined(RED_HAT_LINUX_KERNEL)
#include "blk-rhas21.c"
#else
#error No blk-<ver>.c file!
#endif

#if PAGE_CACHE_SIZE % 1024
#error Oh no, PAGE_CACHE_SIZE is not divisible by 1k! I cannot cope.
#endif

#define IBLOCKS_PER_PAGE  (PAGE_CACHE_SIZE / 512)
#define K_PER_PAGE (PAGE_CACHE_SIZE / 1024)

/* some random number */
#define OSMFS_MAGIC	0x958459f6

/* Debugging is on */
#define DEBUG 1

static struct super_operations osmfs_ops;
static struct address_space_operations osmfs_aops;
static struct file_operations osmfs_dir_operations;
static struct file_operations osmfs_file_operations;
static struct inode_operations osmfs_dir_inode_operations;

static kmem_cache_t	*osm_request_cachep;

/*
 * osmfs super-block data in memory
 */
struct osmfs_sb_info {
	struct super_block *osmfs_super;
	/* Prevent races accessing the used block
	 * counts. Conceptually, this could probably be a semaphore,
	 * but the only thing we do while holding the lock is
	 * arithmetic, so there's no point */
	spinlock_t osmfs_lock;

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

#define OSMFS_SB(sb) ((struct osmfs_sb_info *)((sb)->u.generic_sbp))


struct osmfs_file_info {
	struct file *f_file;
	spinlock_t f_lock;		/* Lock on the structure */
	struct list_head f_ctx;		/* Hook into the i_threads list */
	struct list_head f_ios;		/* Outstanding I/Os for this thread */
	struct list_head f_complete;	/* Completed I/Os for this thread */
	struct list_head f_disks;	/* List of disks opened */
};

#define OSMFS_FILE(_f) ((struct osmfs_file_info *)((_f)->private_data))


#define OSM_HASH_BUCKETS 1024
/*
 * osmfs inode data in memory
 *
 * Note that 'thread' here can mean 'process' too :-)
 */
struct osmfs_inode_info {
	struct inode *i_inode;
	spinlock_t i_lock;		/* lock on the osmfs_inode_info structure */
	struct list_head disk_hash[OSM_HASH_BUCKETS]; /* Hash of disk handles */
	struct list_head i_threads;	/* list of context structures for each calling thread */
};

#define OSMFS_INODE(_i) ((struct osmfs_inode_info *)((_i->u.generic_ip)))



/*
 * osm disk info
 */
struct osm_disk_info {
	struct osmfs_inode_info *d_inode;
	struct list_head d_hash;
	kdev_t d_dev;
	atomic_t d_count;
	struct list_head d_open;	/* List of assocated osm_disk_heads */
	struct list_head d_ios;
};


/*
 * osm disk info lists
 *
 * Each file_info struct has a list of disks it has opened.  As this
 * is an M->N mapping, an intermediary structure is needed
 */
struct osm_disk_head {
	struct osm_disk_info *h_disk;	/* Hook into disk's list */
	struct osmfs_file_info *h_file;	/* Hook into file's list */
	struct list_head h_flist;	/* Pointer to owning file */
	struct list_head h_dlist;	/* Pointer to associated disk */
};


/* OSM I/O requests */
struct osm_request {
	struct list_head r_list;
	struct list_head r_queue;
	struct osmfs_file_info *r_file;
	struct osm_disk_info *r_disk;
	osm_ioc *r_ioc;				/* User osm_ioc */
	__u16 r_status;				/* status_osm_ioc */
	int r_error;
	unsigned long r_elapsed;		/* Start time while in-flight, elapsted time once complete */
	kvec_cb_t r_cb;				/* kvec info */
	struct buffer_head *r_bh;		/* Assocated buffers */
	struct buffer_head *r_bhtail;		/* tail of buffers */
	unsigned long r_bh_count;		/* Number of buffers */
	unsigned long r_seg_count;		/* sg segments */
	atomic_t r_io_count;			/* Atomic count */
};


/*
 * Instance lock has contention in I/O.  We should make this per hash
 * bucket.
 */
static void lock_oi(struct osmfs_inode_info *oi)
{
	spin_lock(&oi->i_lock);
}  /* lock_oi() */

static void unlock_oi(struct osmfs_inode_info *oi)
{
	spin_unlock(&oi->i_lock);
}  /* lock_oi() */



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
 * 1024 bucket table.  The hash?  kdev_t & 0x37777 (last 14 bits).
 * Come up with a better one.  Come up with a better search.  This isn't
 * set in stone, it's just a start.
 */
#define OSM_HASH_DISK(dev) ((dev) & 0x37777)


static inline struct osm_disk_info *osm_find_disk(struct osmfs_inode_info *oi, kdev_t dev)
{
	struct list_head *l, *p;
	struct osm_disk_info *d = NULL;

	printk("OSM: Looking up device 0x%.8lX\n", (unsigned long)dev);
	
	lock_oi(oi);
	l = &(oi->disk_hash[OSM_HASH_DISK(dev)]);
	if (list_empty(l))
		goto out;

	list_for_each(p, l) {
		d = list_entry(p, struct osm_disk_info, d_hash);
		printk("OSM: Comparing device 0x%.8lX\n", (unsigned long) d->d_dev);
		if (d->d_dev == dev) {
			atomic_inc(&d->d_count);
			break;
		}
		d = NULL;
	}

out:
	unlock_oi(oi);
	return d;
}  /* osm_find_disk() */


static inline struct osm_disk_info *osm_add_disk(struct osmfs_inode_info *oi, kdev_t dev)
{
	struct list_head *l, *p;
	struct osm_disk_info *d, *n;

	if (!oi)
		BUG();

	/* FIXME: Maybe a kmem_cache_t later */
	n = kmalloc(sizeof(*n), GFP_KERNEL);
	if (!n)
		return NULL;

	d = NULL;
	lock_oi(oi);
	l = &(oi->disk_hash[OSM_HASH_DISK(dev)]);

	if (list_empty(l))
		goto out;

	list_for_each(p, l) {
		d = list_entry(l, struct osm_disk_info, d_hash);
		if (d->d_dev == dev)
			break;
		d = NULL;
	}
out:
	if (!d) {
		n->d_inode = oi;
		n->d_dev = dev;
		atomic_set(&n->d_count, 1);
		INIT_LIST_HEAD(&n->d_open);
		INIT_LIST_HEAD(&n->d_ios);

		list_add(&n->d_hash, l);
		d = n;
	}
	else
		kfree(n);
	unlock_oi(oi);

	return d;
}  /* osm_add_disk() */


static inline void osm_put_disk(struct osm_disk_info *d)
{
	if (!d)
		BUG();

	if (atomic_dec_and_test(&d->d_count)) {
		printk("OSM: Freeing disk 0x%.8lX\n", (unsigned long)d->d_dev);
		if (!list_empty(&d->d_open))
			BUG();

		kfree(d);
	}
}  /* osm_put_disk() */


#ifdef DEBUG
/* Debugging code */
static void osmdump_file(struct osmfs_file_info *ofi)
{
	struct list_head *p;
	struct osm_disk_head *h;
	struct osm_disk_info *d;
	struct osm_request *r;

	printk("OSM: Dumping osmfs_file_info 0x%p\n", ofi);
	printk("OSM: Opened disks:\n");
	list_for_each(p, &ofi->f_disks) {
		h = list_entry(p, struct osm_disk_head, h_flist);
		d = h->h_disk;
		printk("OSM: 	0x%p [0x%02X, 0x%02X]\n",
		       d, MAJOR(d->d_dev), MINOR(d->d_dev));
	}
	spin_lock(&ofi->f_lock);
	printk("OSM: Pending I/Os:\n");
	list_for_each(p, &ofi->f_ios) {
		r = list_entry(p, struct osm_request, r_list);
		d = r->r_disk;
		printk("OSM:	0x%p (against [0x%02X, 0x%02X])\n",
		       r, MAJOR(d->d_dev), MINOR(d->d_dev));
	}

	printk("OSM: Complete I/Os:\n");
	list_for_each(p, &ofi->f_complete) {
		r = list_entry(p, struct osm_request, r_list);
		printk("OSM:	0x%p\n", r);
	}

	spin_unlock(&ofi->f_lock);
}  /* osmdump_file() */

static void osmdump_inode(struct osmfs_inode_info *oi)
{
	int i;
	struct list_head *p, *q, *hash;
	struct osm_disk_info *d;
	struct osm_disk_head *h;
	struct osmfs_file_info *f;

	printk("OSM: Dumping osmfs_inode_info 0x%p\n", oi);
	printk("OSM: Open threads:\n");
	list_for_each(p, &oi->i_threads) {
		f = list_entry(p, struct osmfs_file_info, f_ctx);
		printk("OSM:	0x%p\n", f);
	}

	printk("OSM: Known disks:\n");
	for (i = 0; i < OSM_HASH_BUCKETS; i++) {
		hash = &(oi->disk_hash[i]);
		if (list_empty(hash))
			continue;
		list_for_each(p, hash) {
			d = list_entry(p, struct osm_disk_info, d_hash);
			printk("OSM: 	0x%p, [0x%02X, 0x%02X]\n",
			       d, MAJOR(d->d_dev), MINOR(d->d_dev));
			printk("OSM:	Owners:\n");
			list_for_each(q, &d->d_open) {
				h = list_entry(q, struct osm_disk_head,
					       h_dlist);
				f = h->h_file;
				printk("OSM:		0x%p\n", f);
			}
		}
	}
}  /* osmdump_inode() */
#endif



/*
 * Resource limit helper functions
 */

static inline void lock_osb(struct osmfs_sb_info *osb)
{
	spin_lock(&(osb->osmfs_lock));
}

static inline void unlock_osb(struct osmfs_sb_info *osb)
{
	spin_unlock(&(osb->osmfs_lock));
}

/* Decrements the free inode count and returns true, or returns false
 * if there are no free inodes */
static int osmfs_alloc_inode(struct super_block *sb)
{
	struct osmfs_sb_info *osb = OSMFS_SB(sb);
	int ret = 1;

	lock_osb(osb);
	if (!osb->max_inodes || osb->free_inodes > 0)
		osb->free_inodes--;
	else
		ret = 0;
	unlock_osb(osb);
	
	return ret;
}

/* Increments the free inode count */
static void osmfs_dealloc_inode(struct super_block *sb)
{
	struct osmfs_sb_info *osb = OSMFS_SB(sb);
	
	lock_osb(osb);
	osb->free_inodes++;
	unlock_osb(osb);
}

/* Decrements the free dentry count and returns true, or returns false
 * if there are no free dentries */
static int osmfs_alloc_dentry(struct super_block *sb)
{
	struct osmfs_sb_info *osb = OSMFS_SB(sb);
	int ret = 1;

	lock_osb(osb);
	if (!osb->max_dentries || osb->free_dentries > 0)
		osb->free_dentries--;
	else
		ret = 0;
	unlock_osb(osb);
	
	return ret;
}

/* Increments the free dentry count */
static void osmfs_dealloc_dentry(struct super_block *sb)
{
	struct osmfs_sb_info *osb = OSMFS_SB(sb);
	
	lock_osb(osb);
	osb->free_dentries++;
	unlock_osb(osb);
}

/* If the given page can be added to the give inode for osmfs, return
 * true and update the filesystem's free page count and the inode's
 * i_blocks field. Always returns true if the file is already used by
 * osmfs (ie. PageDirty(page) is true)  */
int osmfs_alloc_page(struct inode *inode, struct page *page)
{
	struct osmfs_sb_info *osb = OSMFS_SB(inode->i_sb);
	int ret = 1;

	lock_osb(osb);
		
	if ( (osb->free_pages > 0) &&
	     ( !osb->max_file_pages ||
	       (inode->i_data.nrpages <= osb->max_file_pages) ) ) {
		inode->i_blocks += IBLOCKS_PER_PAGE;
		osb->free_pages--;
	} else
		ret = 0;
	
	unlock_osb(osb);

	return ret;
}

void osmfs_dealloc_page(struct inode *inode, struct page *page)
{
	struct osmfs_sb_info *osb = OSMFS_SB(inode->i_sb);

	if (! Page_Uptodate(page))
		return;

	lock_osb(osb);

	ClearPageDirty(page);
	
	osb->free_pages++;
	inode->i_blocks -= IBLOCKS_PER_PAGE;
	
	if (osb->free_pages > osb->max_pages) {
		printk(KERN_ERR "OSM: Error in page allocation, free_pages (%ld) > max_pages (%ld)\n", osb->free_pages, osb->max_pages);
	}

	unlock_osb(osb);
}



static int osmfs_statfs(struct super_block *sb, struct statfs *buf)
{
#if 0
	struct osmfs_sb_info *osb = OSMFS_SB(sb);

	lock_osb(osb);
	buf->f_blocks = osb->max_pages;
	buf->f_files = osb->max_inodes;

	buf->f_bfree = osb->free_pages;
	buf->f_bavail = buf->f_bfree;
	buf->f_ffree = osb->free_inodes;
	unlock_osb(osb);
#endif

	buf->f_type = OSMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_namelen = 255;
	return 0;
}

/*
 * Lookup the data. This is trivial - if the dentry didn't already
 * exist, we know it is negative.
 */
static struct dentry * osmfs_lookup(struct inode *dir, struct dentry *dentry)
{
	d_add(dentry, NULL);
	return NULL;
}

struct inode *osmfs_get_inode(struct super_block *sb, int mode, int dev)
{
	int i;
	struct inode * inode;
	struct osmfs_inode_info *oi;

	if (! osmfs_alloc_inode(sb))
		return NULL;

	inode = new_inode(sb);

	if (inode) {
		oi = kmalloc(sizeof(struct osmfs_inode_info),
			     GFP_KERNEL);
		oi->i_inode = inode;

		for (i = 0; i < OSM_HASH_BUCKETS; i++)
			INIT_LIST_HEAD(&(oi->disk_hash[i]));

		INIT_LIST_HEAD(&oi->i_threads);
		spin_lock_init(&oi->i_lock);
		OSMFS_INODE(inode) = oi;
		if (!oi)
		{
			osmfs_dealloc_inode(sb);
			kfree(inode);
			return NULL;
		}

		inode->i_mode = mode;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_rdev = NODEV;
		inode->i_mapping->a_ops = &osmfs_aops;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_fop = &osmfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &osmfs_dir_inode_operations;
			inode->i_fop = &osmfs_dir_operations;
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
		}
	} else
		osmfs_dealloc_inode(sb);

	return inode;
}

/*
 * File creation. Allocate an inode, update free inode and dentry counts
 * and we're done..
 */
static int osmfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int dev)
{
	struct super_block *sb = dir->i_sb;
	struct inode * inode;
	int error = -ENOSPC;

#if 0  /* ramfs cruft */
	if (! osmfs_alloc_dentry(sb))
		return error;
#endif

	printk("OSM: Alloc'd dentry\n");
	inode = osmfs_get_inode(dir->i_sb, mode, dev);

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);		/* Extra count - pin the dentry in core */
		error = 0;
	} else {
		osmfs_dealloc_dentry(sb);
	}

	return error;
}

static int osmfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
	return osmfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static inline int osmfs_positive(struct dentry *dentry)
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
static int osmfs_empty(struct dentry *dentry)
{
	struct list_head *list;

	spin_lock(&dcache_lock);
	list = dentry->d_subdirs.next;

	while (list != &dentry->d_subdirs) {
		struct dentry *de = list_entry(list, struct dentry, d_child);

		if (osmfs_positive(de)) {
			spin_unlock(&dcache_lock);
			return 0;
		}
		list = list->next;
	}
	spin_unlock(&dcache_lock);
	return 1;
}

/*
 * This works for both directories and regular files.
 * (non-directories will always have empty subdirs)
 */
static int osmfs_unlink(struct inode * dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	int retval = -ENOTEMPTY;

	if (osmfs_empty(dentry)) {
		struct inode *inode = dentry->d_inode;

		inode->i_nlink--;
		dput(dentry);			/* Undo the count from "create" - this does all the work */

		osmfs_dealloc_dentry(sb);

		retval = 0;
	}
	return retval;
}

static void osmfs_delete_inode(struct inode *inode)
{
	osmfs_dealloc_inode(inode->i_sb);

	clear_inode(inode);
}

static void osmfs_put_super(struct super_block *sb)
{
	kfree(sb->u.generic_sbp);
}

struct osmfs_params {
	uid_t uid;
	gid_t gid;
	int mode;
	long inodes;
};

static int parse_options(char * options, struct osmfs_params *p)
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

static void init_limits(struct osmfs_sb_info *osb, struct osmfs_params *p)
{
	struct sysinfo si;

	si_meminfo(&si);

	osb->max_inodes = 0;
	if (p->inodes >= 0)
		osb->max_inodes = p->inodes;

	osb->free_inodes = osb->max_inodes;

	return;
}

/* reset_limits is called during a remount to change the usage limits.

   This will suceed, even if the new limits are lower than current
   usage. This is the intended behaviour - new allocations will fail
   until usage falls below the new limit */
static void reset_limits(struct osmfs_sb_info *osb, struct osmfs_params *p)
{
	lock_osb(osb);

	if (p->inodes >= 0) {
		int used_inodes = osb->max_inodes - osb->free_inodes;

		osb->max_inodes = p->inodes;
		osb->free_inodes = osb->max_inodes - used_inodes;
	}

	unlock_osb(osb);
}

static int osmfs_remount(struct super_block * sb, int * flags, char * data)
{
	struct osmfs_params params;
	struct osmfs_sb_info * osb = OSMFS_SB(sb);

	if (parse_options((char *)data, &params) != 0)
		return -EINVAL;

	reset_limits(osb, &params);

	printk(KERN_DEBUG "OSM: osmfs remounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "OSM:	maxinstances=%ld\n",
	       osb->max_inodes);

	return 0;
}

static unsigned long osm_disk_open(struct osmfs_file_info *ofi, struct osmfs_inode_info *oi, kdev_t dev)
{
	struct osm_disk_info *d;
	struct osm_disk_head *h;

	if (!ofi || !oi)
		BUG();

	h = kmalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return 0UL;

	d = osm_find_disk(oi, dev);
	if (!d) {
		/* FIXME: Need to verify it's a valid device here */
		d = osm_add_disk(oi, dev);
	}

	h->h_disk = d;
	h->h_file = ofi;

	spin_lock(&ofi->f_lock);
	list_add(&h->h_flist, &ofi->f_disks);
	spin_unlock(&ofi->f_lock);

	lock_oi(oi);
	list_add(&h->h_dlist, &d->d_open);
	unlock_oi(oi);

	return d ? (unsigned long)dev : 0UL;
}  /* osm_disk_open() */


static int osm_disk_close(struct osmfs_file_info *ofi, struct osmfs_inode_info *oi, kdev_t dev)
{
	struct osm_disk_info *d;
	struct list_head *p;
	struct osm_disk_head *h;

	if (!ofi || !oi)
		BUG();

	d = osm_find_disk(oi, dev);
	if (!d)
		return -EINVAL;

	spin_lock(&ofi->f_lock);
	h = NULL;
	list_for_each(p, &ofi->f_disks) {
		h = list_entry(p, struct osm_disk_head, h_flist);
		if (h->h_disk == d)
			break;
		h = NULL;
	}
	if (!h)
		BUG();

	list_del(&h->h_flist);
	spin_unlock(&ofi->f_lock);

	lock_oi(oi);
	list_del(&h->h_dlist);

	/* Last close */
	if (list_empty(&d->d_open))
		list_del(&d->d_hash);
	unlock_oi(oi);

	/* Drop the ref from osm_find_disk() */
	osm_put_disk(d);

	/* Real put */
	osm_put_disk(d);

	return 0;
}  /* osm_disk_close() */


static int osm_update_user_ioc(struct osm_request *r)
{
	osm_ioc *ioc;
	__u16 tmp_status;

	ioc = r->r_ioc;
	printk("OSM: User IOC is 0x%p\n", ioc);

	/* Need to get the current userspace bits because OSM_CANCELLED is currently set there */
	printk("OSM: Getting tmp_status\n");
	if (get_user(tmp_status, &(ioc->status_osm_ioc)))
		return -EFAULT;
	r->r_status |= tmp_status;
	printk("OSM: Putting r_status (0x%08X)\n", r->r_status);
	if (put_user(r->r_status, &(ioc->status_osm_ioc)))
		return -EFAULT;
	if (r->r_status & OSM_ERROR) {
		printk("OSM: Putting r_error (0x%08X)\n", r->r_error);
		if (put_user(r->r_error, &(ioc->error_osm_ioc)))
			return -EFAULT;
	}
	if (r->r_status & OSM_COMPLETED) {
		if (put_user(r->r_elapsed, &(ioc->elaptime_osm_ioc)))
			return -EFAULT;
	}
	printk("OSM: r_status:0x%08X, bitmask:0x%08X, combined:0x%08X\n",
	       r->r_status,
	       (OSM_SUBMITTED | OSM_COMPLETED | OSM_ERROR),
	       (r->r_status & (OSM_SUBMITTED | OSM_COMPLETED | OSM_ERROR)));
	if (r->r_status & OSM_FREE) {
		if (put_user(0UL, &(ioc->request_key_osm_ioc)))
			return -EFAULT;
	} else if (r->r_status &
		   (OSM_SUBMITTED | OSM_ERROR)) {
		printk("OSM: Putting key 0x%p\n", r);
		/* Only on first submit */
		if (put_user(r, &(ioc->request_key_osm_ioc)))
			return -EFAULT;
	}

	return 0;
}  /* osm_update_user_ioc() */


static struct osm_request *osm_request_alloc()
{
	return kmem_cache_alloc(osm_request_cachep, GFP_KERNEL);
}  /* osm_request_alloc() */


static void osm_request_free(struct osm_request *r)
{
	/* FIXME: Clean up bh and buffer stuff */

	kmem_cache_free(osm_request_cachep, r);
}  /* osm_request_free() */


static void osm_finish_io(struct osm_request *r)
{
	struct osm_disk_info *d = r->r_disk;
	struct osmfs_file_info *ofi = r->r_file;
	struct osmfs_inode_info *oi = d->d_inode;

	if (!ofi || !d || !oi)
		BUG();

	lock_oi(oi);
	list_del(&r->r_queue);
	r->r_disk = NULL;
	unlock_oi(oi);
	osm_put_disk(d);

	spin_lock(&ofi->f_lock);
	list_del(&r->r_list);
	list_add(&r->r_list, &ofi->f_complete);
	if (r->r_error != 0)
		r->r_status |= OSM_ERROR;
	else
		r->r_status |= OSM_COMPLETED;

	spin_unlock(&ofi->f_lock);
}  /* osm_finish_io() */


static void osm_end_kvec_io(void *_req, struct kvec *vec, ssize_t res)
{
	struct osm_request *r = _req;
	ssize_t err = 0;
	
	if (!r)
		BUG();

	if (res < 0) {
		err = res;
		res = 0;
	}

	while (r->r_bh) {
		struct buffer_head *tmp = r->r_bh;
		r->r_bh = tmp->b_reqnext;
		if (!err) {
		       	if (buffer_uptodate(tmp))
				res += tmp->b_size;
			else
				err = -EIO;
		}
		kmem_cache_free(bh_cachep, tmp);
	}
	r->r_bhtail = NULL;

	if (vec) {
		unmap_kvec(vec, 0);
		free_kvec(vec);
		r->r_cb.vec = NULL;
	}

	switch (err) {
		default:
			printk("OSM: Invalid error of %d!\n", err);
			r->r_error = OSM_ERR_INVAL;
			break;

		case 0:
			break;

		case -EFAULT:
			r->r_error = OSM_ERR_FAULT;
			break;

		case -EIO:
			r->r_error = OSM_ERR_IO;
			break;

		case -ENODEV:
			r->r_error = OSM_ERR_NODEV;
			break;

		case -ENOMEM:
			r->r_error = OSM_ERR_NOMEM;
			break;

		case -EINVAL:
			r->r_error = OSM_ERR_INVAL;
			break;
	}

	osm_finish_io(r);
}  /* osm_end_kvec_io() */


static int osm_submit_request(request_queue_t *q, int rw,
			      struct osm_request *r,
			      unsigned long first, unsigned long nr)
{
	struct request *req;

	printk("OSM: Queue:0x%p, Lock:0x%p\n", q, q->queue_lock);
	printk("OSM: get_request\n");
	req = get_request_wait(q, rw);

	printk("OSM: take lock\n");
	printk("OSM: r_bh_count = %ld\n", r->r_bh_count);

	IO_REQUEST_LOCK(q);

	q->plug_device_fn(q, req->bh->b_rdev); /* is atomic */

	req->cmd = rw;
	req->errors = 0;
	req->hard_sector = req->sector = first;
	req->hard_nr_sectors = req->nr_sectors = nr;
	req->current_nr_sectors = req->hard_cur_sectors =
		r->r_bh->b_size >> 9;
	req->nr_segments = r->r_seg_count;
       	req->nr_hw_segments = 1;
	req->buffer = r->r_bh->b_data;
	req->waiting = NULL;
	req->bh = r->r_bh;
	req->bhtail = r->r_bhtail;
	req->rq_dev = r->r_bh->b_rdev;
	req->start_time = jiffies;

	blk_started_io(req->nr_sectors);

	/*
	 * account_start_io() and friends not handled, because
	 * they suck and are static functions
	 */

	/* FIXME: Do something smarter than "add to end" */
	add_request(q, req, q->queue_head.prev);

	IO_REQUEST_UNLOCK(q);

	return 0;
}  /* osm_submit_request() */


static void osm_end_buffer_io(struct buffer_head *bh, int uptodate)
{
	struct osm_request *r;

	mark_buffer_uptodate(bh, uptodate);

	r = bh->b_private;
	unlock_buffer(bh);

	if (atomic_dec_and_test(&r->r_io_count)) {
		osm_end_kvec_io(r, r->r_cb.vec, 0);
	}
}  /* osm_end_buffer_io() */


static int osm_build_io(request_queue_t *q, int rw,
			struct osm_request *r,
			unsigned long blknr, unsigned long nr)
{
	struct kvec	*vec = r->r_cb.vec;
	struct kveclet	*veclet;
	int		err;
	int		length;
	kdev_t		dev = r->r_disk->d_dev;
	unsigned	sector_size = get_hardsect_size(dev);
	int		i;

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
			printk("OSM: osm_build_io: tuple[%d]->offset=0x%x length=0x%x sector_size: 0x%x\n", i, veclet->offset, veclet->length, sector_size);
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
		printk("OSM: osm_build_io: !i\n");
		return -EINVAL;
	}

	r->r_bh_count = r->r_seg_count = 0;

	/* This is massaged from fs/buffer.c.  Blame Ben. */
	/* This is ugly.  FIXME. */
	for (i=0, veclet=vec->veclet; i<vec->nr; i++,veclet++) {
		struct page *page = veclet->page;
		unsigned offset = veclet->offset;
		unsigned length = veclet->length;
		unsigned iosize = PAGE_SIZE;
	       
		if (length < PAGE_SIZE)
			iosize = sector_size;

		iosize = length;

		if (!page)
			BUG();

		while (length > 0) {
			struct buffer_head *tmp;
			tmp = kmem_cache_alloc(bh_cachep, GFP_NOIO);
			err = -ENOMEM;
			if (!tmp)
				goto error;

			tmp->b_dev = B_FREE;
			tmp->b_size = iosize;
			set_bh_page(tmp, page, offset);
			tmp->b_this_page = tmp;

			init_buffer(tmp, osm_end_buffer_io, NULL);

			/* This is why it has to be a real disk */
			tmp->b_dev = tmp->b_rdev = dev;
			tmp->b_blocknr = tmp->b_rsector = blknr;

			blknr += (iosize / sector_size);
			tmp->b_state = (1 << BH_Mapped) | (1 << BH_Lock)
					| (1 << BH_Req);
			tmp->b_private = r;

			if (rw == WRITE) {
				set_bit(BH_Uptodate, &tmp->b_state);
				clear_bit(BH_Dirty, &tmp->b_state);
			}

			tmp->b_reqnext = NULL;

			tmp = blk_queue_bounce(q, rw, tmp);

			if (r->r_bh) {
				if (!BH_CONTIG(r->r_bhtail, tmp) ||
				    !BH_PHYS_4G(r->r_bhtail, tmp)) {
					r->r_seg_count++;
				}

				r->r_bhtail->b_reqnext = tmp;
				r->r_bhtail = tmp;
			} else {
				r->r_bh = r->r_bhtail = tmp;
				r->r_seg_count = 1;
			}
			r->r_bh_count++;

			length -= iosize;
			offset += iosize;

#if 0  /* Um, this shouldn't matter, should it? */
			if (offset >= PAGE_SIZE) {
				offset = 0;
				break;
			}

			if (brw_cb->nr >= blocks)
				goto submit;
#endif
		} /* End of block loop */
	} /* End of page loop */		

	atomic_set(&r->r_io_count, r->r_bh_count);

	return 0;

error:
	/* Walk bh list, freeing them */
	while (r->r_bh) {
		struct buffer_head *tmp = r->r_bh;
		r->r_bh = tmp->b_reqnext;
		kmem_cache_free(bh_cachep, tmp);
	}
	r->r_bhtail = NULL;

	return err;
}  /* osm_build_io() */


static int osm_submit_io(struct osmfs_file_info *ofi, 
			 struct osmfs_inode_info *oi,
			 osm_ioc *ioc)
{
	int rw;
	int ret;
	struct osm_request *r;
	struct osm_disk_info *d;
	osm_ioc tmp;
	size_t count;
	request_queue_t *q;

	if (copy_from_user(&tmp, ioc, sizeof(tmp)))
		return -EFAULT;

	r = osm_request_alloc();
	if (!r)
		return -ENOMEM;

	r->r_status = 0;
	r->r_file = ofi;
	r->r_ioc = ioc;
	r->r_error = 0;
	r->r_bh = NULL;
	r->r_bhtail = NULL;
	r->r_cb.vec = NULL;
	INIT_LIST_HEAD(&r->r_queue);

	spin_lock(&ofi->f_lock);
	list_add(&r->r_list, &ofi->f_ios);
	spin_unlock(&ofi->f_lock);

	ret = -ENODEV;
	d = osm_find_disk(oi, REAL_HANDLE(tmp.disk_osm_ioc));
	if (!d)
		goto out_error;

	r->r_disk = d;

	lock_oi(oi);
	list_add(&r->r_queue, &d->d_ios);
	unlock_oi(oi);

	count = tmp.rcount_osm_ioc * get_hardsect_size(d->d_dev);

	printk("first, 0x%08lX.%08lX; masked 0x%08lX\n",
	       HIGH_UB4(tmp.first_osm_ioc), LOW_UB4(tmp.first_osm_ioc),
	       (unsigned long)tmp.first_osm_ioc);
	/* linux only supports unsigned long size sector numbers */
	ret = -EINVAL;
	if (tmp.status_osm_ioc ||
	    (tmp.buffer_osm_ioc != (unsigned long)tmp.buffer_osm_ioc) ||
	    (tmp.first_osm_ioc != (unsigned long)tmp.first_osm_ioc) ||
	    (tmp.rcount_osm_ioc != (unsigned long)tmp.rcount_osm_ioc) ||
	    (count > OSM_MAX_IOSIZE) ||
	    (count < 0))
		goto out_error;

	printk("Passed checks\n");

	switch (tmp.operation_osm_ioc) {
		default:
			goto out_error;
			break;

		case OSM_READ:
			rw = READ;
			break;

		case OSM_WRITE:
			rw = WRITE;
			break;

		case OSM_NOOP:
			/* FIXME: Do something */
			return 0;
			break;
	}
	
	/* FIXME: More size checks */

	r->r_cb.vec = map_user_kvec(rw, tmp.buffer_osm_ioc, count);
	r->r_cb.data = r;
	r->r_cb.fn = osm_end_kvec_io;
	if (IS_ERR(r->r_cb.vec)) {
		ret = PTR_ERR(r->r_cb.vec);
		r->r_cb.vec = NULL;
		goto out_error;
	}

	ret = -ENODEV;
	q = blk_get_queue(r->r_disk->d_dev);
	if (!q) {
		printk(KERN_ERR
		       "OSM: osm_submit_request: Attempt to access nonexistent block device [0x%02X, 0x%02X]\n", MAJOR(r->r_bh->b_rdev), MINOR(r->r_bh->b_rdev));
		goto out_error;
	}

	ret = osm_build_io(q, rw, r,
			   tmp.first_osm_ioc, tmp.rcount_osm_ioc);
	printk("OSM: Return from buildio is %d\n", ret);
	if (ret)
		goto out_error;

	ret = osm_submit_request(q, rw, r,
				 tmp.first_osm_ioc, tmp.rcount_osm_ioc);
	if (ret) 
		goto out_error;

	r->r_status |= OSM_SUBMITTED;

out:
	ret = osm_update_user_ioc(r);

	return ret;

out_error:
	osm_end_kvec_io(r, r->r_cb.vec, ret);
	goto out;
}  /* osm_submit_io() */


static int osm_maybe_wait_io(struct osmfs_file_info *ofi, 
			     struct osmfs_inode_info *oi,
			     osm_ioc *iocp)
{
	long ret;
	unsigned long p;
	struct osm_request *r;

	printk("OSM: Entering wait_io()\n");
	if (get_user(p, &(iocp->request_key_osm_ioc)))
		return -EFAULT;

	printk("OSM: User key is 0x%p\n", (struct osm_request *)p);
	r = (struct osm_request *)p;
	if (!r)
		return -EINVAL;

	spin_lock(&ofi->f_lock);
	/* Is it valid? */
	if (!r->r_file || (r->r_file != ofi) ||
	    list_empty(&r->r_list)) {
		spin_unlock(&ofi->f_lock);
		return -EINVAL;
	}

	printk("OSM: osm_request is valid...we think\n");
	while (!(r->r_status & (OSM_COMPLETED |
				OSM_BUSY | OSM_ERROR))) {
		spin_unlock(&ofi->f_lock);
		/* FIXME: Wait correctly and deal with timeout */
		schedule();
		spin_lock(&ofi->f_lock);
	}


	if (list_empty(&ofi->f_complete))
		BUG();
#ifdef DEBUG
	{
		/* Check that the request is on ofi->f_complete */
		struct list_head *p;
		struct osm_request *q;
		q = NULL;
		list_for_each(p, &ofi->f_complete) {
			q = list_entry(p, struct osm_request, r_list);
			if (q == r)
				break;
			q = NULL;
		}
		if (!q)
			BUG();
	}
#endif  /* DEBUG */

	printk("OSM: Removing request 0x%p\n", r);
	list_del_init(&r->r_list);
	r->r_file = NULL;
	spin_unlock(&ofi->f_lock);

	r->r_status |= OSM_FREE;

	ret = osm_update_user_ioc(r);

	printk("OSM: Freeing request 0x%p\n", r);
	osm_request_free(r);

	return ret;
}  /* osm_maybe_wait_io() */


static int osm_complete_io(struct osmfs_file_info *ofi, 
			   struct osmfs_inode_info *oi,
			   osm_ioc **ioc)
{
	int ret = 0;
	struct list_head *l;
	struct osm_request *r;

	spin_lock(&ofi->f_lock);

	if (list_empty(&ofi->f_complete)) {
		spin_unlock(&ofi->f_lock);
		*ioc = NULL;
		return 0;
	}

	l = ofi->f_complete.prev;
	r = list_entry(l, struct osm_request, r_list);
	list_del_init(&r->r_list);
	r->r_file = NULL;
	spin_unlock(&ofi->f_lock);

	r->r_status |= OSM_FREE;

	*ioc = r->r_ioc;
	
#ifdef DEBUG
	if (!(r->r_status & (OSM_FREE | OSM_ERROR)))
		BUG();
#endif /* DEBUG */

	ret = osm_update_user_ioc(r);

	osm_request_free(r);

	return ret;
}  /* osm_complete_io() */


/* HACK HACK HACK */
/* This just completes I/Os to test the code */
static void hack_io(struct osmfs_file_info *ofi)
{
	struct list_head *l, *p;
	struct osm_request *r;

	list_for_each_safe(l, p, &ofi->f_ios) {
		r = list_entry(l, struct osm_request, r_list);
		osm_finish_io(r);
	}
}  /* END hack_io() HACK */


static int osm_do_io(struct osmfs_file_info *ofi,
		     struct osmfs_inode_info *oi,
		     struct osmio *ioc)
{
	int ret = 0;
	__u32 i;
	__u32 status = 0;
	osm_ioc *iocp;

#if 0
	/* HACK HACK HACK */
	/* Without actual I/O, this fakes I/O completion */

	printk("Pre-hack_io()\n");
	osmdump_file(ofi);
	hack_io(ofi);
	printk("Post-hack_io()\n");
	osmdump_file(ofi);
#else
	printk("OSM: Entering osm_do_io()\n");
	osmdump_file(ofi);
#endif

	/* END HACK */


	ret = 0;
	if (ioc->requests) {
		for (i = 0; i < ioc->reqlen; i++) {
			if (get_user(iocp, ioc->requests + i))
				return -EFAULT;
			ret = osm_submit_io(ofi, oi, iocp);
			if (ret)
				goto out;
		}
	}

	if (ioc->waitreqs) {
		for (i = 0; i < ioc->waitlen; i++) {
			if (get_user(iocp, ioc->waitreqs + i))
				return -EFAULT;

			ret = osm_maybe_wait_io(ofi, oi, iocp);
			if (ret)
				goto out;
		}
		status |= OSM_IO_WAITED;
	}

	if (ioc->completions) {
		for (i = 0; i < ioc->complen; i++) {
			ret = osm_complete_io(ofi, oi, &iocp);
			if (ret)
				goto out;
			if (iocp) {
				ret = put_user(iocp,
					       ioc->completions + i);
			} else {
				i--; /* Reset this completion */
				if (ioc->waitreqs)
					break;
				/* FIXME: Wait on some I/Os */
				schedule();
			}
		}
		if (i >= ioc->complen)
			status |= OSM_IO_FULL;
	}

out:
	if (ret == -EINTR) {
		ret = 0;
		status |= OSM_IO_POSTED;
	} else if (ret == -ETIMEDOUT) {
		ret = 0;
		status |= OSM_IO_TIMEOUT;
	}

	if (put_user(status, ioc->statusp))
		return -EFAULT;
	return ret;
}  /* osm_do_io() */


static int osmfs_file_open(struct inode * inode, struct file * file)
{
	struct osmfs_inode_info * oi;
	struct osmfs_file_info * ofi;

	if (OSMFS_FILE(file))
		BUG();

	ofi = (struct osmfs_file_info *)kmalloc(sizeof(*ofi),
						GFP_KERNEL);
	if (!ofi)
		return -ENOMEM;
	ofi->f_file = file;
	spin_lock_init(&ofi->f_lock);
	INIT_LIST_HEAD(&ofi->f_ctx);
	INIT_LIST_HEAD(&ofi->f_disks);
	INIT_LIST_HEAD(&ofi->f_ios);
	INIT_LIST_HEAD(&ofi->f_complete);

	oi = OSMFS_INODE(inode);
	lock_oi(oi);
	list_add(&ofi->f_ctx, &oi->i_threads);
	unlock_oi(oi);

	OSMFS_FILE(file) = ofi;

	return 0;
}  /* osmfs_file_open() */


static int osmfs_file_release(struct inode * inode, struct file * file)
{
	struct osmfs_inode_info *oi;
	struct osmfs_file_info *ofi;
	struct list_head *p, *q;
	struct osm_disk_head *h;
	struct osm_disk_info *d;
	struct osm_request *r;

	oi = OSMFS_INODE(inode);
	ofi = OSMFS_FILE(file);
	OSMFS_FILE(file) = NULL;

	if (!ofi)
		BUG();

	/*
	 * Shouldn't need the lock, no one else has a reference
	 * osm_disk_close will need to take it when completing I/O
	 */
	list_for_each_safe(p, q, &ofi->f_disks) {
		h = list_entry(p, struct osm_disk_head, h_flist);
		d = h->h_disk;
		/* FIXME: Should wait on outstanding I/O */
		osm_disk_close(ofi, oi, d->d_dev);
	}

	/* FIXME: Clean up things that hang off of ofi */

	lock_oi(oi);
	list_del(&ofi->f_ctx);
	unlock_oi(oi);

	spin_lock(&ofi->f_lock);
	while (!list_empty(&ofi->f_ios)) {
		printk("OSM: There are still I/Os on ofi 0x%p\n", ofi);
		spin_unlock(&ofi->f_lock);
		/* FIXME: Needs a better wait */
		schedule();
		spin_lock(&ofi->f_lock);
	}
	
	/* I don't *think* we need the lock here anymore, but... */

	/* Clear unreaped I/Os */
	while (!list_empty(&ofi->f_complete)) {
		p = ofi->f_complete.prev;
		r = list_entry(p, struct osm_request, r_list);
		list_del(&r->r_list);
		r->r_file = NULL;
		osm_request_free(r);
	}
	spin_unlock(&ofi->f_lock);

	kfree(ofi);

	return 0;
}  /* osmfs_file_release() */


static int osmfs_file_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	kdev_t kdv;
	struct gendisk *g;
	int drive, first_minor, dev;
	unsigned long handle;
	struct osmfs_file_info *ofi = OSMFS_FILE(file);
	struct osmfs_inode_info *oi = OSMFS_INODE(inode);
	struct osmio ioc;

	switch (cmd) {
		default:
			return -ENOTTY;
			break;

		case OSMIOC_ISDISK:
			if (get_user(dev, (int *)arg))
				return -EFAULT;
			kdv = to_kdev_t(dev);
			printk("OSM: Checking disk %d,%d\n", MAJOR(kdv), MINOR(kdv));
			/* Right now we trust only SCSI ->request_fn */
			if (!SCSI_DISK_MAJOR(MAJOR(kdv)))
				return -EINVAL;

			g = get_gendisk(kdv);
			if (!g)
				return -ENXIO;
			drive = (MINOR(kdv) >> g->minor_shift);
			first_minor = (drive << g->minor_shift);
			if (first_minor != MINOR(kdv))
				return -EINVAL;
			break;

		case OSMIOC_OPENDISK:
			if (get_user(handle, (unsigned long *)arg))
				return -EFAULT;
			kdv = to_kdev_t(handle);
			handle = osm_disk_open(ofi, oi, kdv);
			printk("OSM: Opened handle 0x%.8lX\n", handle);
			return put_user(handle, (unsigned long *)arg);
			break;

		case OSMIOC_CLOSEDISK:
			if (get_user(handle, (unsigned long *)arg))
				return -EFAULT;
			printk("OSM: Closing handle 0x%.8lX\n", handle);
			return osm_disk_close(ofi, oi, handle);
			break;

		case OSMIOC_IODISK:
			if (copy_from_user(&ioc, (struct osmio *)arg,
					   sizeof(ioc)))
				return -EFAULT;
			return osm_do_io(ofi, oi, &ioc);
			break;

		case OSMIOC_DUMP:
			/* Dump data */
			osmdump_file(ofi);
			osmdump_inode(oi);
			break;
	}

	return 0;
}

static int osmfs_dir_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	unsigned long new_iid;
	struct osmfs_sb_info *osb = OSMFS_SB(inode->i_sb);

	switch (cmd) {
		default:
			return -ENOTTY;
			break;

		case OSMIOC_GETIID:
			lock_osb(osb);
			new_iid = osb->next_iid;
			osb->next_iid++;
			unlock_osb(osb);
			return put_user(new_iid, (unsigned long *)arg);
			break;

                case OSMIOC_CHECKIID:
                        if (get_user(new_iid, (long *)arg))
                            return -EFAULT;
                        lock_osb(osb);
                        if (new_iid >= osb->next_iid)
                            new_iid = 0;
                        unlock_osb(osb);
                        if (!new_iid)
                            return put_user(new_iid, (long *)arg);
                        break;
	}

	return 0;
}

static int osmfs_sync_file(struct file * file, struct dentry *dentry, int datasync)
{
	return 0;
}

static struct address_space_operations osmfs_aops = {
};

static struct file_operations osmfs_file_operations = {
	.open		= osmfs_file_open,
	.release	= osmfs_file_release,
	.ioctl		= osmfs_file_ioctl,
};

static struct file_operations osmfs_dir_operations = {
	.read		= generic_read_dir,
	.readdir	= dcache_readdir,
	.fsync		= osmfs_sync_file,
	.ioctl		= osmfs_dir_ioctl,
};

static struct inode_operations osmfs_dir_inode_operations = {
	.create		= osmfs_create,
	.lookup		= osmfs_lookup,
	.unlink		= osmfs_unlink,
};

static struct super_operations osmfs_ops = {
	.statfs		= osmfs_statfs,
	.put_inode	= force_delete,
	.delete_inode	= osmfs_delete_inode,
	.put_super	= osmfs_put_super,
	.remount_fs     = osmfs_remount,
};

/*
 * Initialisation
 */

static struct super_block *osmfs_read_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct dentry * root;
	struct osmfs_sb_info * osb;
	struct osmfs_params params;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = OSMFS_MAGIC;
	sb->s_op = &osmfs_ops;
	sb->s_maxbytes = MAX_NON_LFS;	/* Why? */

	osb = kmalloc(sizeof(struct osmfs_sb_info), GFP_KERNEL);
	OSMFS_SB(sb) = osb;
	if (!osb)
		return NULL;

	osb->osmfs_super = sb;
	osb->next_iid = 1;
	spin_lock_init(&osb->osmfs_lock);

	if (parse_options((char *)data, &params) != 0)
		return NULL;

	init_limits(osb, &params);

	inode = osmfs_get_inode(sb, S_IFDIR | 0755, 0);
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

	printk(KERN_DEBUG "OSM: osmfs mounted with options: %s\n", 
	       data ? (char *)data : "<defaults>" );
	printk(KERN_DEBUG "OSM:	uid=%d gid=%d mode=0%o maxinstances=%ld\n",
	       params.uid, params.gid, params.mode,
	       osb->max_inodes);
	return sb;
}

static DECLARE_FSTYPE(osmfs_fs_type, "osmfs", osmfs_read_super, FS_LITTER);

static int __init init_osmfs_fs(void)
{
	osm_request_cachep =
		kmem_cache_create("osm_request",
				  sizeof(struct osm_request),
				  0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!osm_request_cachep)
		panic("Unable to create osm_request cache\n");

	return register_filesystem(&osmfs_fs_type);
}

static void __exit exit_osmfs_fs(void)
{
	unregister_filesystem(&osmfs_fs_type);
	kmem_cache_destroy(osm_request_cachep);
}

module_init(init_osmfs_fs)
module_exit(exit_osmfs_fs)
MODULE_LICENSE("GPL");

