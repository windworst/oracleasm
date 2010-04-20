#
# RHEL 4
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = el4
INCLUDE_REQUIRES = glibc-kernheaders

$(TOPDIR)/vendor/rhel4/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

	# Build large SMP variants if 2.6.9-34
	LARGESMP = $(if $(call lt,$(get_release_major),34),1,0)

	# Build xenU for versions less than 55
	XEN = $(if $(call lt,$(get_release_major),55),1,0)

$(TOPDIR)/vendor/rhel4/oracleasm-%.spec: $(TOPDIR)/vendor/rhel4/oracleasm.spec-generic
	$(spec_subst) > $@

rhel4_%_srpm: $(TOPDIR)/vendor/rhel4/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/rhel4/oracleasm-$(patsubst rhel4_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/rhel4/oracleasm-$(patsubst rhel4_%_srpm,%,$@).spec

rhel4_%_rpm: rhel4_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst rhel4_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel4/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/rhel4/kernel.guess targets
