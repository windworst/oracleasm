TOPDIR = ..

include $(TOPDIR)/Preamble.make

DEFINES = -DMODULE -DLINUX -D__KERNEL__ 

# Red Hat kernel magic
# Note that arch is still retrieved from /boot/kernel.h.
# Set __MODULE_KERNEL_<arch> on the make command line to override.
ifeq ($(KVER),smp)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0
  DEFINES += -D__BOOT_KERNEL_HUGEMEM=0
  DEFINES += -D__BOOT_KERNEL_SMP=1 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
# Ugh - Silly Kurt
ifeq ($(KVER),ent)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=1
  DEFINES += -D__BOOT_KERNEL_HUGEMEM=0
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),enterprise)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=1
  DEFINES += -D__BOOT_KERNEL_HUGEMEM=0
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),hugemem)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0
  DEFINES += -D__BOOT_KERNEL_HUGEMEM=1
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=0 
endif
ifeq ($(KVER),up)
  DEFINES += -D__BOOT_KERNEL_ENTERPRISE=0
  DEFINES += -D__BOOT_KERNEL_HUGEMEM=0 
  DEFINES += -D__BOOT_KERNEL_SMP=0 
  DEFINES += -D__BOOT_KERNEL_UP=1 
endif

INCLUDES = -I. -I$(TOPDIR)/include -I$(KERNELINC) -I$(GCCINC)

WARNINGS = -Wall -Wstrict-prototypes

CPPFLAGS = -nostdinc $(MODVERSIONS)
CFLAGS += -fomit-frame-pointer $(WARNINGS) $(MCMODEL) $(M64)
LDADD = -nostdlib

MODULES = oracleasm.o

SOURCES = oracleasm.c
OBJECTS = $(subst .c,.o,$(SOURCES))

HEADERS =					\
	$(TOPDIR)/include/linux/asmkernel.h	\
	$(TOPDIR)/include/linux/asmcompat32.h	\
	$(TOPDIR)/include/linux/asmabi.h	\
	$(TOPDIR)/include/linux/asmdisk.h	\
	$(TOPDIR)/include/asmerror.h

oracleasm.o: $(HEADERS)

INSTALL_RULES = install-asm

install-asm: oracleasm.o
	$(TOPDIR)/mkinstalldirs $(DESTDIR)$(MODULEDIR)
	$(INSTALL_DATA) oracleasm.o $(DESTDIR)$(MODULEDIR)/oracleasm.o

DIST_FILES = $(SOURCES)

include $(TOPDIR)/Postamble.make
