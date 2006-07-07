#
# Header files
#

# This Vendor.make expects TOOLSARCH set by someone including it
# It also expects PKG_VERSION and INCLUDE_REQUIRES to be set.


RPMBUILD = $(shell /usr/bin/which rpmbuild 2>/dev/null || /usr/bin/which rpm 2>/dev/null || echo /bin/false)
RPM_TOPDIR = `pwd`

$(TOPDIR)/oracleasm-headers-$(DIST_VERSION)-$(PKG_VERSION).src.rpm: dist $(TOPDIR)/vendor/common/oracleasm-headers.spec-generic
	sed -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' -e 's/@@INCLUDE_REQUIRES@@/'$(INCLUDE_REQUIRES)'/' < "$(TOPDIR)/vendor/common/oracleasm-headers.spec-generic" > "$(TOPDIR)/vendor/common/oracleasm-headers.spec"
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/common/oracleasm-headers.spec"
	rm "$(TOPDIR)/vendor/common/oracleasm-headers.spec"

headers_srpm: $(TOPDIR)/oracleasm-headers-$(DIST_VERSION)-$(PKG_VERSION).src.rpm

headers_rpm: headers_srpm
	$(RPMBUILD) --rebuild $(TOOLSARCH) "oracleasm-headers-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"

