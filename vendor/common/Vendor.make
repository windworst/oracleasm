#
# Support files
#

# This Vendor.make expects TOOLSARCH set by someone including it


RPMBUILD = $(shell /usr/bin/which rpmbuild 2>/dev/null || /usr/bin/which rpm 2>/dev/null || echo /bin/false)
RPM_TOPDIR = `pwd`

$(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(PKG_VERSION).src.rpm: dist $(TOPDIR)/vendor/common/oracleasm-support.spec-generic
	sed -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' < "$(TOPDIR)/vendor/common/oracleasm-support.spec-generic" > "$(TOPDIR)/vendor/common/oracleasm-support.spec"
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/common/oracleasm-support.spec"
	rm "$(TOPDIR)/vendor/common/oracleasm-support.spec"

support_srpm: $(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(PKG_VERSION).src.rpm

support_rpm: support_srpm
	$(RPMBUILD) --rebuild $(TOOLSARCH) "oracleasm-support-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"

