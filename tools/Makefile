TOPDIR = ..

include $(TOPDIR)/Preamble.make

SBIN_PROGRAMS = asmtool

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS += -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS += -Wall $(USERM64)

INCLUDES = -I. -I$(TOPDIR)/include

SOURCES = asmtool.c
LOCAL_HEADERS = list.h
HEADERS =					\
	$(TOPDIR)/include/oratypes.h		\
	$(TOPDIR)/include/osmlib.h		\
	$(TOPDIR)/include/linux/asmdisk.h

OBJECTS = $(subst .c,.o,$(SOURCES))

DIST_FILES = $(SOURCES) $(LOCAL_HEADERS)

asmtool.o: $(HEADERS) $(LOCAL_HEADERS)

asmtool: $(OBJECTS)
	$(LINK)

include $(TOPDIR)/Postamble.make
