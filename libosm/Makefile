TOPDIR = ..

include $(TOPDIR)/Preamble.make

LIBRARIES = libosm.so

VERSION_FLAGS = -DVERSION="\"${VERSION}\""
CPPFLAGS = -DLINUX ${VERSION_FLAGS} $(DEFS)
CFLAGS = -Wall -g -O2 -fPIC

INCLUDES = -I. -I$(TOPDIR)/include

SOURCES = asmlib.c
HEADERS =					\
	$(TOPDIR)/include/oratypes.h		\
	$(TOPDIR)/include/osmlib.h		\
	$(TOPDIR)/include/asmerror.h		\
	$(TOPDIR)/include/linux/asmcompat32.h	\
	$(TOPDIR)/include/linux/osmabi.h	\
	$(TOPDIR)/include/linux/osmdisk.h

OBJECTS = $(subst .c,.o,$(SOURCES))

DIST_FILES = $(SOURCES)

asmlib.o: $(HEADERS)

libosm.so: $(OBJECTS)
	$(LINK) -shared

include $(TOPDIR)/Postamble.make
