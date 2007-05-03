#
# RHEL 3
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel3/rpmarch.guess tools $(TOPDIR))
VENDOR_EXTENSION = el3

# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-kernheaders
include $(TOPDIR)/vendor/common/Vendor.make

packages: headers_rpm
