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
SUBDIRS = include libosm kernel tools test vendor

#
# Extra (non-source) files to distribute
#
DIST_FILES = \
	COPYING		\
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


rhas_srpm: dist
	rpm -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.9-e.spec"

rhas_rpm: rhas_srpm
	rpm --rebuild --target i686 "oracleasm-2.4.9-e-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

rhel3_srpm: dist
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" "$(TOPDIR)/vendor/redhat/oracleasm-2.4.21-EL.spec"

rhel3_rpm: rhel3_srpm
	rpmbuild --rebuild --target i686 "oracleasm-2.4.21-EL-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_smp_%_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-64GB-SMP-$(patsubst ul10_smp_%_srpm,%,$@).spec

ul10_smp_%_rpm: ul10_smp_%_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-64GB-SMP-$(patsubst ul10_smp_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_psmp_%_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-4GB-SMP-$(patsubst ul10_psmp_%_srpm,%,$@).spec

ul10_psmp_%_rpm: ul10_psmp_%_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-4GB-SMP-$(patsubst ul10_psmp_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"

ul10_dflt_%_srpm: dist
	rpm -bs --define "_sourcedir `realpath $(TOPDIR)`" --define "_srcrpmdir `realpath $(TOPDIR)`" $(TOPDIR)/vendor/unitedlinux/oracleasm-2.4.19-4GB-$(patsubst ul10_dflt_%_srpm,%,$@).spec

ul10_dflt_%_rpm: ul10_dflt_%_srpm
	rpm --rebuild --target i586 "oracleasm-2.4.19-4GB-$(patsubst ul10_dflt_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
