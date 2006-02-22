# See if we are being included by 2.6 kbuild
ifeq ($(KERNELRELEASE),)
TOPDIR = ..

include $(TOPDIR)/Preamble.make
else #ifeq ($(KERNELRELEASE),)
# We are included by kbuild

ORACLEASM_SRC_DIR	:= $(M)

include $(ORACLEASM_SRC_DIR)/../Config.make
endif


HEADERS = transaction_file.h proc.h masklog.h
SOURCES = oracleasm.c transaction_file.c proc.c masklog.c
OBJECTS = $(subst .c,.o,$(SOURCES))

ifeq ($(KERNELRELEASE),)
# Called as "make" part of makefile.  This executes kbuild

INSTALL_MODULE = oracleasm.ko

ifeq ($(BUILD_DRIVER), yes)
ALL_RULES = build-oracleasm

CLEAN_RULES = clean-oracleasm

INSTALL_RULES = install-oracleasm
endif

build-oracleasm:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules

clean-oracleasm:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) clean

install-oracleasm: $(INSTALL_MODULE)
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules_install

DIST_FILES = $(SOURCES) $(HEADERS)

include $(TOPDIR)/Postamble.make

else
# Called from kbuild

STAMP_DIR = $(ORACLEASM_SRC_DIR)

INCLUDES = -I. -I$(ORACLEASM_SRC_DIR)/../include
EXTRA_CFLAGS += $(DEFINES) $(INCLUDES)

obj-m += oracleasm.o
#oracleasm-objs := $(OBJECTS)

endif
