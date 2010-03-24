#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = .

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make

TOOLSARCH = $(shell $(TOPDIR)/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/rpmarch.guess module $(TOPDIR))

#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = include kernel documents vendor

KAPI_COMPAT_FILES =					\
	kapi-compat/include/bio_end_io.h		\
	kapi-compat/include/bio_map_user.h		\
	kapi-compat/include/blk_limits.h		\
	kapi-compat/include/blk_segments.h		\
	kapi-compat/include/blkdev_get.h		\
	kapi-compat/include/blkdev_put.h		\
	kapi-compat/include/current_creds.h		\
	kapi-compat/include/i_blksize.h			\
	kapi-compat/include/i_mutex.h			\
	kapi-compat/include/i_private.h			\
	kapi-compat/include/kmem_cache_create.h		\
	kapi-compat/include/kmem_cache_s.h		\
	kapi-compat/include/slab_ctor_three_arg.h	\
	kapi-compat/include/slab_ctor_two_arg.h		\
	kapi-compat/include/slab_ctor_verify.h

#
# Extra (non-source) files to distribute
#
DIST_FILES = \
	COPYING.GPL	\
	COPYING.LGPL	\
	README		\
	Config.make.in	\
	Preamble.make	\
	Postamble.make	\
	aclocal.m4	\
	kfeature.m4	\
	mbvendor.m4	\
	config.guess	\
	config.sub	\
	configure	\
	configure.in	\
	install-sh	\
	mkinstalldirs	\
	svnrev.guess	\
	Vendor.make	\
	vendor.guess	\
	$(KAPI_COMPAT_FILES)

DIST_RULES = dist-subdircreate

dist-subdircreate:
	$(TOPDIR)/mkinstalldirs $(DIST_DIR)/kapi-compat/include
	


include Vendor.make

#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
