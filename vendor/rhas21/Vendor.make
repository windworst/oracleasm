#
# RHEL 3
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhas21/rpmarch.guess tools $(TOPDIR))

include $(TOPDIR)/vendor/common/Vendor.make

packages: support_rpm
