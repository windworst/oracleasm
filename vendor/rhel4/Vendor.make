#
# RHEL 4
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess module $(TOPDIR))

$(TOPDIR)/vendor/rhel4/oracleasm-2.6.9-%.spec: $(TOPDIR)/vendor/rhel4/oracleasm-2.6.9-EL.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-2.6.9-}"; \
		SPECVER="$${SPECVER%.EL.spec}"; \
		sed -e 's/@@SVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

rhel4_%_srpm: dist $(TOPDIR)/vendor/rhel4/oracleasm-2.6.9-%.EL.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/rhel4/oracleasm-2.6.9-$(patsubst rhel4_%_srpm,%,$@).EL.spec

rhel4_%_rpm: rhel4_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-2.6.9-$(patsubst rhel4_%_rpm,%,$@).EL-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"


include $(TOPDIR)/vendor/common/Vendor.make
