#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = ../..

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make


#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = 

HEADERS =			\
	asmabi.h		\
	asmabi_compat.h		\
	asmdisk.h		\
	asmmanager.h		\
	asmmanager_compat.h	\
	asmkernel.h		\
	asmcompat32.h		\
	asm_module_version.h

DIST_FILES = $(HEADERS) asm_module_version.h.in


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
