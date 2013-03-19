#
# RHEL6
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel6/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel6/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = $(shell $(TOPDIR)/vendor/rhel6/rpmdist.guess)
INCLUDE_REQUIRES = glibc-kernheaders

$(TOPDIR)/vendor/rhel6/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

$(TOPDIR)/vendor/rhel6/oracleasm-%.spec: $(TOPDIR)/vendor/rhel6/oracleasm.spec-generic
	$(spec_subst) > $@

rhel6_%_srpm: $(TOPDIR)/vendor/rhel6/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/rhel6/oracleasm-$(patsubst rhel6_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/rhel6/oracleasm-$(patsubst rhel6_%_srpm,%,$@).spec

rhel6_%_rpm: rhel6_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst rhel6_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel6/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/rhel6/kernel.guess targets
