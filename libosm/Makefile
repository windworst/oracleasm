TOPDIR = ..

include $(TOPDIR)/Preamble.make

LIBRARIES = libosm.so

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS = -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS = -Wall -g -O2 -fPIC

INCLUDES = -I. -I$(TOPDIR)/include

SOURCES = osmlib.c
HEADERS =					\
	$(TOPDIR)/include/oratypes.h		\
	$(TOPDIR)/include/osmlib.h		\
	$(TOPDIR)/include/osmerror.h		\
	$(TOPDIR)/include/linux/osmcompat32.h	\
	$(TOPDIR)/include/linux/osmabi.h	\
	$(TOPDIR)/include/linux/osmdisk.h

OBJECTS = $(subst .c,.o,$(SOURCES))

DIST_FILES = $(SOURCES)

osmlib.o: $(HEADERS)

libosm.so: $(OBJECTS)
	$(LINK) -shared

include $(TOPDIR)/Postamble.make
