#
# Support files
#

$(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm: $(TOPDIR)/vendor/common/oracleasm-support.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/common/oracleasm-support.spec"

support_srpm: $(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm

support_rpm: support_srpm
	$(RPMBUILD) --rebuild $(TOOLSARCH) "oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

