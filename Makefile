#
# Makefile for osmlib
#

.config:
	@./Configure

include .config

OSMLIB_SRCS = libosm/osmlib.c
OSMLIB_OBJS = libosm/osmlib.o
OSMLIB_INCLUDES = -I include
OSMLIB_CPPFLAGS = -DLINUX $(OSMLIB_INCLUDES)

OSMLIB_HEADERS =			\
	include/osmlib.h		\
	include/arch/osmstructures.h	\
	include/osmprivate.h

OSMTEST_SRCS = test/osmtest.c
OSMTEST_OBJS = test/osmtest.o
OSMTEST_INCLUDES = $(OSMLIB_INCLUDES)
OSMTEST_CPPFLAGS = -DLINUX $(OSMTEST_INCLUDES)
OSMTEST_LDFLAGS = libosm/libosm.a # -L libosm -losm

CPPFLAGS = -g -O2 -Wall

KERNEL_INCLUDES = -I $(KERNEL_INCLUDE_PATH) $(OSMLIB_INCLUDES)
KERNEL_DEFS = -D__KERNEL__ -DMODULE -DLINUX -DRED_HAT_LINUX_KERNEL=1
KERNEL_CPPFLAGS = $(KERNEL_INCLUDES) $(KERNEL_DEFS)

# FIXME: Needs selection
KVEC_FILE = kernel/kiovec-rhas21.c
BLK_FILE = kernel/blk-rhas21.c

all: kernel/osm.o libosm/libosm.so test/osmtest-bin

distclean: clean
	rm -f .config
	rm -f include/arch

clean:
	rm -f $(OSMLIB_OBJS) $(OSMTEST_OBJS) \
		libosm/libosm.so libosm/libosm.a kernel/osm.o \
		test/osmtest-bin libosm-virtual-stamp

$(OSMLIB_OBJS): $(OSMLIB_HEADERS)
$(OSMLIB_OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(OSMLIB_CPPFLAGS) -c -o $@ $<

kernel/osm.o: include/osmprivate.h $(KVEC_FILE) $(BLK_FILE)
kernel/osm.o: kernel/osm.c
	$(CC) $(CPPFLAGS) $(KERNEL_CPPFLAGS) -c -o $@ $<

libosm-virtual-stamp: libosm/libosm.so libosm/libosm.a
	touch libosm-virtual-stamp

libosm/libosm.so: $(OSMLIB_OBJS)
	$(CC) -shared $(LDFLAGS) $(LIBADD) -o $@ $^

libosm/libosm.a: $(OSMLIB_OBJS)
	ar cru $@ $^

test/osmtest.o: test/osmtest.c
	$(CC) $(CPPFLAGS) $(OSMTEST_CPPFLAGS) -c -o $@ $<

test/osmtest-bin: libosm-virtual-stamp test/osmtest.o
	$(CC) -o $@ $(OSMTEST_OBJS) $(OSMTEST_LDFLAGS)
