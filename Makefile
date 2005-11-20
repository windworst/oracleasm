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
	config.guess	\
	config.sub	\
	configure	\
	configure.in	\
	install-sh	\
	mkinstalldirs	\
	rpmarch.guess



include Vendor.make

#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
