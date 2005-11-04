TOPDIR = ..

include $(TOPDIR)/Preamble.make

SBIN_PROGRAMS = asmtool asmscan

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS += -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS += -Wall $(USERM64)

INCLUDES = -I. -I$(TOPDIR)/include

ASMTOOL_SOURCES = asmtool.c devmap.c
ASMSCAN_SOURCES = asmscan.c

LOCAL_HEADERS = list.h devmap.h
HEADERS =					\
	$(TOPDIR)/include/linux/asmdisk.h	\
	$(TOPDIR)/include/linux/asmmanager.h

ASMTOOL_OBJECTS = $(subst .c,.o,$(ASMTOOL_SOURCES))
ASMSCAN_OBJECTS = $(subst .c,.o,$(ASMSCAN_SOURCES))

DIST_FILES = $(ASMTOOL_SOURCES) $(ASMSCAN_SOURCES) $(LOCAL_HEADERS) \
	oracleasm_debug_link

INSTALL_RULES += install-debug-link

install-debug-link:
	$(SHELL) $(TOPDIR)/mkinstalldirs $(DESTDIR)$(exec_prefix)/lib/oracleasm
	$(INSTALL_PROGRAM) oracleasm_debug_link $(DESTDIR)$(exec_prefix)/lib/oracleasm

asmtool: $(ASMTOOL_OBJECTS)
	$(LINK)

asmscan: $(ASMSCAN_OBJECTS)
	$(LINK)

include $(TOPDIR)/Postamble.make
