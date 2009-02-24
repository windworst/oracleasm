#
# SLES 10
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles10/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles10/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = SLE10


$(TOPDIR)/vendor/sles10/oracleasm-%.spec: $(TOPDIR)/vendor/sles10/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

sles10_%_srpm: dist $(TOPDIR)/vendor/sles10/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/sles10/oracleasm-$(patsubst sles10_%_srpm,%,$@).spec

sles10_%_rpm: sles10_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst sles10_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).$(VENDOR_EXTENSION).src.rpm"


# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-devel
include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles10/kernel.guess targets) headers_rpm
