#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = ../../..

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make


#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = 

HEADERS_SUBDIR = linux/oracleasm
HEADERS =			\
	abi.h			\
	abi_compat.h		\
	disk.h			\
	error.h			\
	manager.h		\
	manager_compat.h	\
	kernel.h		\
	compat32.h		\
	module_version.h

DIST_FILES = $(HEADERS) module_version.h.in


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
