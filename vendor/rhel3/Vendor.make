#
# RHEL 3
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel3/rpmarch.guess tools $(TOPDIR))

include $(TOPDIR)/vendor/common/Vendor.make

packages: support_rpm
