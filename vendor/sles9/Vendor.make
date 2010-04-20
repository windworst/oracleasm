#
# SLES 9
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = SLE9
INCLUDE_REQUIRES = glibc-devel

$(TOPDIR)/vendor/sles9/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

$(TOPDIR)/vendor/sles9/oracleasm-%.spec: $(TOPDIR)/vendor/sles9/oracleasm.spec-generic dist
	$(spec_subst) > $@

sles9_%_srpm: $(TOPDIR)/vendor/sles9/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/sles9/oracleasm-$(patsubst sles9_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/sles9/oracleasm-$(patsubst sles9_%_srpm,%,$@).spec

sles9_%_rpm: sles9_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst sles9_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles9/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/sles9/kernel.guess targets
