#
# RHEL 4
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess module $(TOPDIR))

$(TOPDIR)/vendor/rhel4/oracleasm-%.spec: $(TOPDIR)/vendor/rhel4/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

rhel4_%_srpm: dist $(TOPDIR)/vendor/rhel4/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/rhel4/oracleasm-$(patsubst rhel4_%_srpm,%,$@).spec

rhel4_%_rpm: rhel4_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst rhel4_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"


include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel4/kernel.guess targets) support_rpm
