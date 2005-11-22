#
# SLES 9
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess module $(TOPDIR))


$(TOPDIR)/vendor/sles9/oracleasm-%.spec: $(TOPDIR)/vendor/sles9/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < $< > $@

sles9_%_srpm: dist $(TOPDIR)/vendor/sles9/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/sles9/oracleasm-$(patsubst sles9_%_srpm,%,$@).spec

sles9_%_rpm: sles9_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst sles9_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"


include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/sles9/kernel.guess targets) support_rpm
