TOPDIR = ..

include $(TOPDIR)/Preamble.make

DEFINES = -DMODULE -DLINUX -D__KERNEL__ 

# Red Hat kernel magic
ifeq ($(KVER),smp)
  DEFINES += -D__MODULE_KERNEL_i686=1 
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0 
  DEFINES += -D__BOOT_KERNEL_SMP=1 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),ent)
  DEFINES += -D__MODULE_KERNEL_i686=1 
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=1 
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),up)
  DEFINES += -D__MODULE_KERNEL_i686=1 
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

INSTALL_RULES = install-osm

install-ocfs: osm.o
	$(TOPDIR)/mkinstalldirs $(DESTDIR)$(MODULEDIR)
	$(INSTALL_DATA) osm.o $(DESTDIR)$(MODULEDIR)/osm.o

DIST_FILES = $(SOURCES)

include $(TOPDIR)/Postamble.make
