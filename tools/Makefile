TOPDIR = ..

include $(TOPDIR)/Preamble.make

SBIN_PROGRAMS = asmtool asmscan

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS += -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS += -Wall $(USERM64)

INCLUDES = -I. -I$(TOPDIR)/include

ASMTOOL_SOURCES = asmtool.c
ASMSCAN_SOURCES = asmscan.c

LOCAL_HEADERS = list.h
HEADERS =					\
	$(TOPDIR)/include/linux/asmdisk.h	\
	$(TOPDIR)/include/linux/asmmanager.h

ASMTOOL_OBJECTS = $(subst .c,.o,$(ASMTOOL_SOURCES))
ASMSCAN_OBJECTS = $(subst .c,.o,$(ASMSCAN_SOURCES))

DIST_FILES = $(ASMTOOL_SOURCES) $(ASMSCAN_SOURCES) $(LOCAL_HEADERS)

asmtool: $(ASMTOOL_OBJECTS)
	$(LINK)

asmscan: $(ASMSCAN_OBJECTS)
	$(LINK)

include $(TOPDIR)/Postamble.make
