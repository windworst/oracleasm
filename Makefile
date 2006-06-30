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
SUBDIRS = include kernel tools documents vendor

KAPI_COMPAT_FILES =			\
	kapi-compat/include/i_mutex.h

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
