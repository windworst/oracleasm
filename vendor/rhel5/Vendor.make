#
# RHEL5
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel5/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel5/rpmarch.guess module $(TOPDIR))

$(TOPDIR)/vendor/rhel5/oracleasm-%.spec: $(TOPDIR)/vendor/rhel5/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

rhel5_%_srpm: dist $(TOPDIR)/vendor/rhel5/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/rhel5/oracleasm-$(patsubst rhel5_%_srpm,%,$@).spec

rhel5_%_rpm: rhel5_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst rhel5_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"


# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-kernheaders
include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel5/kernel.guess targets) headers_rpm
