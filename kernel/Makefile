TOPDIR = ..

include $(TOPDIR)/Preamble.make

DEFINES = -DMODULE -DLINUX -D__KERNEL__ 

# Red Hat kernel magic
# Note that arch is still retrieved from /boot/kernel.h.
# Set __MODULE_KERNEL_<arch> on the make command line to override.
ifeq ($(KVER),smp)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0 
  DEFINES += -D__BOOT_KERNEL_SMP=1 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),ent)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=1 
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),up)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0 
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=1 
endif

INCLUDES = -I. -I$(TOPDIR)/include -I$(KERNELINC) -I$(GCCINC)

WARNINGS = -Wall -Wstrict-prototypes

CPPFLAGS = -nostdinc $(MODVERSIONS)
CFLAGS = -O2 -g -fomit-frame-pointer $(WARNINGS)
LDADD = -nostdlib

MODULES = osm.o

SOURCES = osm.c
OBJECTS = $(subst .c,.o,$(SOURCES))

HEADERS =					\
	$(TOPDIR)/include/linux/osmkernel.h	\
	$(TOPDIR)/include/linux/osmcompat32.h	\
	$(TOPDIR)/include/linux/osmabi.h	\
	$(TOPDIR)/include/linux/osmdisk.h	\
	$(TOPDIR)/include/osmerror.h

osm.o: $(HEADERS)

INSTALL_RULES = install-osm

install-ocfs: osm.o
	$(TOPDIR)/mkinstalldirs $(DESTDIR)$(MODULEDIR)
	$(INSTALL_DATA) osm.o $(DESTDIR)$(MODULEDIR)/osm.o

DIST_FILES = $(SOURCES)

include $(TOPDIR)/Postamble.make
