#
# SLES 11
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles11/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles11/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = SLE11


$(TOPDIR)/vendor/sles11/oracleasm-%.spec: $(TOPDIR)/vendor/sles11/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

sles11_%_srpm: dist $(TOPDIR)/vendor/sles11/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/sles11/oracleasm-$(patsubst sles11_%_srpm,%,$@).spec

sles11_%_rpm: sles11_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst sles11_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).$(VENDOR_EXTENSION).src.rpm"


# Package required for /usr/include/linux
INCLUDE_REQUIRES = linux-kernel-headers
include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles11/kernel.guess targets) headers_rpm
