#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = .

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make


#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = libosm kernel tools tests

#
# Extra (non-source) files to distribute
#
DIST_FILES = \
	COPYING		\
	README		\
	Config.make.in	\
	Preamble.make	\
	Postamble.make	\
	config.guess	\
	config.sub	\
	configure	\
	configure.in	\
	install-sh	\
	mkinstalldirs

#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
