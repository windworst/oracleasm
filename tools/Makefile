TOPDIR = ..

include $(TOPDIR)/Preamble.make

BIN_PROGRAMS = osmtool

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS = -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS = -Wall -g -O2

INCLUDES = -I. -I$(TOPDIR)/include

SOURCES = osmtool.c
HEADERS =					\
	$(TOPDIR)/include/oratypes.h		\
	$(TOPDIR)/include/osmlib.h		\
	$(TOPDIR)/include/linux/osmabi.h	\
	$(TOPDIR)/include/linux/osmdisk.h

OBJECTS = $(subst .c,.o,$(SOURCES))

DIST_FILES = $(SOURCES)

osmtool.o: $(HEADERS)

osmtool: $(OBJECTS)
	$(LINK)

include $(TOPDIR)/Postamble.make
