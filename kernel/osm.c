/*
 * osm - Kernel driver for the Oracle Storage Manager
 *
 * Copyright (C) 2002 Oracle Corporation.  All rights reserved.
 *
 * This driver's filesystem code is based on the osmfs filesystem.
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

#include <asm/uaccess.h>
#include <linux/spinlock.h>

#include "arch/osmkernel.h"
#include "osmprivate.h"

#if PAGE_CACHE_SIZE % 1024
#error Oh no, PAGE_CACHE_SIZE is not divisible by 1k! I cannot cope.
#endif

#define IBLOCKS_PER_PAGE  (PAGE_CACHE_SIZE / 512)
#define K_PER_PAGE (PAGE_CACHE_SIZE / 1024)

/* some random number */
#define OSMFS_MAGIC	0x958459f6

static struct super_operations osmfs_ops;
static struct address_space_operations osmfs_aops;
static struct file_operations osmfs_dir_operations;
static struct file_operations osmfs_file_operations;
static struct inode_operations osmfs_dir_inode_operations;

/*
 * osmfs super-block data in memory
 */
struct osmfs_sb_info {
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
	spinlock_t i_lock;		/* lock on the osmfs_inode_info structure */
	struct list_head disk_hash[OSM_HASH_BUCKETS]; /* Hash of disk handles */
	struct list_head i_threads;	/* list of context structures for each calling thread */
};

#define OSMFS_INODE(_i) ((struct osmfs_inode_info *)((_i->u.generic_ip)))



/*
 * osm disk info
 */
struct osm_disk_info {
	struct list_head d_hash;
	kdev_t dev;
	atomic_t d_count;
	struct list_head d_open;	/* List of assocated osm_disk_heads */
};


/*
 * osm disk info lists
 *
 * Each file_info struct has a list of disks it has opened.  As this
 * is an N->N mapping, an intermediary structure is needed
 */
struct osm_disk_head {
	struct osm_disk_info *h_disk;	/* Hook into disk's list */
	struct osmfs_file_info *h_file;	/* Hook into file's list */
	struct list_head h_flist;	/* Pointer to owning file */
	struct list_head h_dlist;	/* Pointer to associated disk */
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
		printk("OSM: Comparing device 0x%.8lX\n", (unsigned long) d->dev);
		if (d->dev == dev) {
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
		if (d->dev == dev)
			break;
		d = NULL;
	}
out:
	if (!d) {
		n->dev = dev;
		atomic_set(&n->d_count, 1);
		INIT_LIST_HEAD(&n->d_open);

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

	if (!atomic_dec_and_test(&d->d_count)) {
		printk("OSM: Freeing disk 0x%.8lX\n", (unsigned long)d->dev);
		if (!list_empty(&d->d_open))
			BUG();

		kfree(d);
	}
}  /* osm_put_disk() */


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

	if (! osmfs_alloc_dentry(sb))
		return error;

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
		osm_disk_close(ofi, oi, d->dev);
	}

	/* FIXME: Clean up things that hang off of ofi */

	lock_oi(oi);
	list_del(&ofi->f_ctx);
	unlock_oi(oi);


	kfree(ofi);

	return 0;
}  /* osmfs_file_release() */


#if 1
/* Debugging code */
static void osmdump_file(struct osmfs_file_info *ofi)
{
	struct list_head *p;
	struct osm_disk_head *h;
	struct osm_disk_info *d;

	printk("OSM: Dumping osmfs_file_info 0x%p\n", ofi);
	printk("OSM: Opened disks:\n");
	list_for_each(p, &ofi->f_disks) {
		h = list_entry(p, struct osm_disk_head, h_flist);
		d = h->h_disk;
		printk("OSM: 	0x%p [0x%02X, 0x%02X]\n",
		       d, MAJOR(d->dev), MINOR(d->dev));
	}
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
			       d, MAJOR(d->dev), MINOR(d->dev));
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


static int osmfs_file_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	kdev_t kdv;
	struct gendisk *g;
	int drive, first_minor, dev;
	unsigned long handle;
	struct osmfs_file_info *ofi;
	struct osmfs_inode_info *oi;

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
			ofi = OSMFS_FILE(file);
			oi = OSMFS_INODE(inode);
			handle = osm_disk_open(ofi, oi, kdv);
			printk("OSM: Opened handle 0x%.8lX\n", handle);
			return put_user(handle, (unsigned long *)arg);
			break;

		case OSMIOC_CLOSEDISK:
			if (get_user(handle, (unsigned long *)arg))
				return -EFAULT;
			ofi = OSMFS_FILE(file);
			oi = OSMFS_INODE(inode);
			printk("OSM: Closing handle 0x%.8lX\n", handle);
			return osm_disk_close(ofi, oi, handle);
			break;

		case OSMIOC_IODISK:
			break;

		case OSMIOC_DUMP:
			/* Dump data */
			ofi = OSMFS_FILE(file);
			oi = OSMFS_INODE(inode);
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
	return register_filesystem(&osmfs_fs_type);
}

static void __exit exit_osmfs_fs(void)
{
	unregister_filesystem(&osmfs_fs_type);
}

module_init(init_osmfs_fs)
module_exit(exit_osmfs_fs)
MODULE_LICENSE("GPL");

