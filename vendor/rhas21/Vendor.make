#
# RHEL 3
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhas21/rpmarch.guess tools $(TOPDIR))

# Package required for /usr/include/linux
INCLUDE_REQUIRES = kernel-headers
include $(TOPDIR)/vendor/common/Vendor.make

packages: headers_rpm
