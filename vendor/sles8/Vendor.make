#
# SLES 9
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles8/rpmarch.guess tools $(TOPDIR))


include $(TOPDIR)/vendor/common/Vendor.make

packages: support_rpm
