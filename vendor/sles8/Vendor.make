#
# SLES 8
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles8/rpmarch.guess tools $(TOPDIR))
VENDOR_EXTENSION = SLE8

# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-devel
include $(TOPDIR)/vendor/common/Vendor.make

packages: headers_rpm
