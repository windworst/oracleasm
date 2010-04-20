#
# RHEL5
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel5/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel5/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = el5
INCLUDE_REQUIRES = glibc-kernheaders

$(TOPDIR)/vendor/rhel5/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

	# If kernel is bigger than 2.6.18 append "-uname-r" to BuildRequires
	KERNEL_REQUIRES = $(if $(call gt,$(get_version_minor),18),-uname-r)

	# If kernel is 2.6.18 build Xen modules
	XEN = $(if $(call eq,$(get_version_minor),18),1,0)

	# If kernel is smaller than 2.6.18 release 53 build debug modules
	DEBUG = $(if $(call and,\
		$(call eq,$(get_version_minor),18),\
		$(call lt,$(get_release_major),53)),1,0)

$(TOPDIR)/vendor/rhel5/oracleasm-%.spec: $(TOPDIR)/vendor/rhel5/oracleasm.spec-generic
	$(spec_subst) > $@

rhel5_%_srpm: $(TOPDIR)/vendor/rhel5/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/rhel5/oracleasm-$(patsubst rhel5_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/rhel5/oracleasm-$(patsubst rhel5_%_srpm,%,$@).spec

rhel5_%_rpm: rhel5_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst rhel5_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel5/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/rhel5/kernel.guess targets
