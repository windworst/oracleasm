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

$(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm: $(TOPDIR)/vendor/redhat/oracleasm-support.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-support.spec"

support_srpm: $(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm

support_rpm: support_srpm
	rpmbuild --rebuild --target $(TOOLSARCH) "oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

rhas_srpm: dist support_srpm $(TOPDIR)/vendor/redhat/oracleasm-2.4.9-e.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.9-e.spec"

rhas_rpm: rhas_srpm support_rpm
	rpmbuild --rebuild --target $(MODULEARCH) "oracleasm-2.4.9-e-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

rhel3_srpm: dist support_srpm $(TOPDIR)/vendor/redhat/oracleasm-2.4.21-EL.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.21-EL.spec"

rhel3_rpm: rhel3_srpm support_rpm
	rpmbuild --rebuild --target i686 "oracleasm-2.4.21-EL-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_%_smp_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-64GB-SMP-$(patsubst ul10_%_smp_srpm,%,$@).spec

ul10_%_smp_rpm: ul10_%_smp_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-64GB-SMP-$(patsubst ul10_%_smp_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_%_psmp_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-4GB-SMP-$(patsubst ul10_%_psmp_srpm,%,$@).spec

ul10_%_psmp_rpm: ul10_%_psmp_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-4GB-SMP-$(patsubst ul10_%_psmp_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_%_deflt_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-4GB-$(patsubst ul10_%_deflt_srpm,%,$@).spec

ul10_%_deflt_rpm: ul10_%_deflt_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-4GB-$(patsubst ul10_%_deflt_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10sp3_%_srpm: dist
	rpm -bs --define "_sourcedir `pwd`" --define "_srcrpmdir `pwd`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.21-$(patsubst ul10sp3_%_srpm,%,$@).spec

ul10sp3_%_rpm: ul10sp3_%_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.21-$(patsubst ul10sp3_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
