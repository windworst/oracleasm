#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = .

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make


#
# Add any directories to recurse into via the SUBDIRS variable.
# 
SUBDIRS = include kernel tools documents vendor

#
# Extra (non-source) files to distribute
#
DIST_FILES = \
	COPYING.GPL	\
	COPYING.LGPL	\
	README		\
	Config.make.in	\
	Preamble.make	\
	Postamble.make	\
	config.guess	\
	config.sub	\
	configure	\
	configure.in	\
	install-sh	\
	mkinstalldirs

$(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm: $(TOPDIR)/vendor/common/oracleasm-support.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/common/oracleasm-support.spec"

support_srpm: $(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm

support_rpm: support_srpm
	$(RPMBUILD) --rebuild --target $(TOOLSARCH) "oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

rhas_srpm: dist support_srpm $(TOPDIR)/vendor/redhat/oracleasm-2.4.9-e.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.9-e.spec"

rhas_rpm: rhas_srpm support_rpm
	$(RPMBUILD) --rebuild --target $(MODULEARCH) "oracleasm-2.4.9-e-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

rhel3_srpm: dist support_srpm $(TOPDIR)/vendor/redhat/oracleasm-2.4.21-EL.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.21-EL.spec"

rhel3_rpm: rhel3_srpm support_rpm
	$(RPMBUILD) --rebuild --target $(MODULEARCH) "oracleasm-2.4.21-EL-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


$(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.21-%.spec: $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.21.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-2.4.21-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/^%define sver.*%{generic}$$/%define sver		'$${SPECVER}'/' < $< > $@

ul10sp3_%_srpm: dist support_srpm $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.21-%.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.21-$(patsubst ul10sp3_%_srpm,%,$@).spec

ul10sp3_%_rpm: ul10sp3_%_srpm support_rpm
	$(RPMBUILD) --rebuild --target $(MODULEARCH) "oracleasm-2.4.21-$(patsubst ul10sp3_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
