#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = ..

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make


#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = linux

HEADERS =		\
	asmerror.h

DIST_FILES = $(HEADERS)

#
# Handle the architecture link
#
SUBARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
				  -e s/arm.*/arm/ -e s/sa110/arm/ \
				  -e s/s390x/s390/ )
ARCH := $(SUBARCH)

#
# Deprecated
#asm-link: asm
#
#asm:
#	$(LN_S) asm-$(ARCH) asm

	
#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
