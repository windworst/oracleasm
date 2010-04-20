#
# SLES 10
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles10/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles10/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = SLE10
INCLUDE_REQUIRES = glibc-devel

$(TOPDIR)/vendor/sles10/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

$(TOPDIR)/vendor/sles10/oracleasm-%.spec: $(TOPDIR)/vendor/sles10/oracleasm.spec-generic dist
	$(spec_subst) > $@

sles10_%_srpm: $(TOPDIR)/vendor/sles10/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/sles10/oracleasm-$(patsubst sles10_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/sles10/oracleasm-$(patsubst sles10_%_srpm,%,$@).spec

sles10_%_rpm: sles10_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst sles10_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles10/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/sles10/kernel.guess targets
