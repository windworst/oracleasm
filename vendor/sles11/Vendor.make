#
# SLES 11
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles11/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles11/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = SLE11
INCLUDE_REQUIRES = glibc-devel

$(TOPDIR)/vendor/sles11/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

$(TOPDIR)/vendor/sles11/oracleasm-%.spec: $(TOPDIR)/vendor/sles11/oracleasm.spec-generic dist
	$(spec_subst) > $@

sles11_%_srpm: $(TOPDIR)/vendor/sles11/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/sles11/oracleasm-$(patsubst sles11_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/sles11/oracleasm-$(patsubst sles11_%_srpm,%,$@).spec

sles11_%_rpm: sles11_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst sles11_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles11/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/sles11/kernel.guess targets
