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
	include/osmprivate.h		\
	include/osmerror.h

COMMON_HEADERS =				\
	include/osmlib.h			\
	include/oratypes.h			\
	include/osmerror.h			\
	include/osmprivate.h			\

ARCH_X86_HEADERS =				\
	include/arch-i386/osmkernel.h		\
	include/arch-i386/osmstructures.h

OSMTOOL_SRCS = tools/osmtool.c
OSMTOOL_OBJS = tools/osmtool.o
OSMTOOL_INCLUDES = $(OSMLIB_INCLUDES)
OSMTOOL_CPPFLAGS = -D LINUX $(OSMTOOL_INCLUDES)
OSMTOOL_LDFLAGS =

OSMTEST_SRCS = test/osmtest.c
OSMTEST_OBJS = test/osmtest.o
OSMTEST_INCLUDES = $(OSMLIB_INCLUDES)
OSMTEST_CPPFLAGS = -DLINUX $(OSMTEST_INCLUDES)
OSMTEST_LDFLAGS = libosm/libosm.a # -L libosm -losm

OSMTEST_MULTI_SRCS = test/osmtest-multi.c
OSMTEST_MULTI_OBJS = test/osmtest-multi.o
OSMTEST_MULTI_INCLUDES = $(OSMLIB_INCLUDES)
OSMTEST_MULTI_CPPFLAGS = -DLINUX $(OSMTEST_INCLUDES)
OSMTEST_MULTI_LDFLAGS = libosm/libosm.a # -L libosm -losm

OSMPROFILE_SRCS = test/osmprofile.c
OSMPROFILE_OBJS = test/osmprofile.o
OSMPROFILE_INCLUDES = $(OSMLIB_INCLUDES)
OSMPROFILE_CPPFLAGS = -DLINUX $(OSMPROFILE_INCLUDES) \
	-I ../aio/libaio-oracle
OSMPROFILE_LDFLAGS = libosm/libosm.a \
	../aio/libaio-oracle/libaio-oracle.a -laio

CPPFLAGS = -g -O2 -Wall

KERNEL_SRCS = kernel/osm.c
KERNEL_INCLUDES = -I $(KERNEL_INCLUDE_PATH) $(OSMLIB_INCLUDES)
KERNEL_DEFS = -D__KERNEL__ -DMODULE -DLINUX #-DRED_HAT_LINUX_KERNEL=1
KERNEL_CPPFLAGS = $(KERNEL_INCLUDES) $(KERNEL_DEFS)

# FIXME: Needs selection
BLK_SRCS = 			\
	kernel/blk-rhas21.c
BLK_FILE = kernel/blk-rhas21.c

OSM_SOURCES =			\
	$(OSMLIB_SRCS)		\
	$(OSMTEST_SRCS)		\
	$(OSMTEST_MULTI_SRCS)	\
	$(BLK_SRCS)		\
	$(KERNEL_SRCS)

TEST_WRAPPERS =			\
	test/osmtest-multi	\
	test/osmtest

DIST_FILES =			\
	Makefile		\
	README			\
	TODO			\
	Configure

all:				\
	kernel/osm.o		\
	libosm/libosm.so	\
	tools/osmtool		\
	test/osmtest-bin	\
	test/osmtest-multi-bin	\
	test/osmprofile-bin

dist:
	@mkdir $(DISTNAME)
	@mkdir $(DISTNAME)/include
	@mkdir $(DISTNAME)/include/arch-i386
	@mkdir $(DISTNAME)/libosm
	@mkdir $(DISTNAME)/test
	@mkdir $(DISTNAME)/kernel
	@cp $(DIST_FILES) $(DISTNAME)
	@cp $(COMMON_HEADERS) $(DISTNAME)/include
	@cp $(ARCH_X86_HEADERS) $(DISTNAME)/include/arch-i386
	@cp $(OSMLIB_SRCS) $(DISTNAME)/libosm
	@cp $(OSMTEST_SRCS) $(OSMTEST_MULTI_SRCS) $(OSMPROFILE_SRCS) $(TEST_WRAPPERS) $(DISTNAME)/test
	@cp $(KERNEL_SRCS) $(BLK_SRCS) $(DISTNAME)/kernel
	tar -czvf $(DISTNAME).tar.gz $(DISTNAME)
	@rm -rf $(DISTNAME)

distclean: clean
	rm -f .config
	rm -f include/arch
	rm -f osmlib-linux*.tar.gz

clean:
	rm -f $(OSMLIB_OBJS) $(OSMTEST_OBJS) $(OSMTEST_MULTI_OBJS) \
		$(OSMPROFILE_OBJS) $(OSMTOOL_OBJS) \
		libosm/libosm.so libosm/libosm.a kernel/osm.o \
		test/osmtest-bin test/osmtest-multi-bin \
		test/osmprofile tools/osmtool \
		libosm-virtual-stamp

$(OSMLIB_OBJS): $(OSMLIB_HEADERS)
$(OSMLIB_OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(OSMLIB_CPPFLAGS) -c -o $@ $<

kernel/osm.o: include/osmprivate.h $(BLK_FILE)
kernel/osm.o: kernel/osm.c
	$(CC) $(CPPFLAGS) $(KERNEL_CPPFLAGS) -c -o $@ $<

libosm-virtual-stamp: libosm/libosm.so libosm/libosm.a
	touch libosm-virtual-stamp

libosm/libosm.so: $(OSMLIB_OBJS)
	$(CC) -shared $(LDFLAGS) $(LIBADD) -o $@ $^

libosm/libosm.a: $(OSMLIB_OBJS)
	ar cru $@ $^

tools/osmtool.o: tools/osmtool.c
	$(CC) $(CPPFLAGS) $(OSMTOOL_CPPFLAGS) -c -o $@ $<

tools/osmtool: $(OSMTOOL_OBJS)
	$(CC) -o $@ $(OSMTOOL_OBJS) $(OSMTOOL_LDFLAGS)

test/osmtest.o: test/osmtest.c
	$(CC) $(CPPFLAGS) $(OSMTEST_CPPFLAGS) -c -o $@ $<

test/osmtest-bin: libosm-virtual-stamp test/osmtest.o
	$(CC) -o $@ $(OSMTEST_OBJS) $(OSMTEST_LDFLAGS)

test/osmtest-multi.o: test/osmtest-multi.c
	$(CC) $(CPPFLAGS) $(OSMTEST_MULTI_CPPFLAGS) -c -o $@ $<

test/osmtest-multi-bin: libosm-virtual-stamp test/osmtest-multi.o
	$(CC) -o $@ $(OSMTEST_MULTI_OBJS) $(OSMTEST_MULTI_LDFLAGS)

test/osmprofile.o: test/osmprofile.c
	$(CC) $(CPPFLAGS) $(OSMPROFILE_CPPFLAGS) -c -o $@ $<
	
test/osmprofile-bin: libosm-virtual-stamp test/osmprofile.o
	$(CC) -o $@ $(OSMPROFILE_OBJS) $(OSMPROFILE_LDFLAGS)
