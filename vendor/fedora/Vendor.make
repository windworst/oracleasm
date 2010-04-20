#
# Fedora
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/fedora/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/fedora/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = $(shell $(TOPDIR)/vendor/fedora/rpmdist.guess)
INCLUDE_REQUIRES = glibc-kernheaders

$(TOPDIR)/vendor/fedora/oracleasm-%.spec: \
	KERNEL_VERSION = $(get_version_release)

$(TOPDIR)/vendor/fedora/oracleasm-%.spec: $(TOPDIR)/vendor/fedora/oracleasm.spec-generic
	$(spec_subst) > $@

fedora_%_srpm: $(TOPDIR)/vendor/fedora/oracleasm-%.spec dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" \
		$(TOPDIR)/vendor/fedora/oracleasm-$(patsubst fedora_%_srpm,%,$@).spec
	@rm $(TOPDIR)/vendor/fedora/oracleasm-$(patsubst fedora_%_srpm,%,$@).spec

fedora_%_rpm: fedora_%_srpm
	rpmbuild --rebuild $(MODULEARCH) \
		"oracleasm-$(patsubst fedora_%_rpm,%,$@)-$(DPV).src.rpm"

include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/fedora/kernel.guess targets) headers_rpm

targets:
	$(TOPDIR)/vendor/fedora/kernel.guess targets
