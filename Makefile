#
# Set TOPDIR to the project toplevel directory
# 
TOPDIR = .

#
# Include this first, right after setting TOPDIR
#
include $(TOPDIR)/Preamble.make

TOOLSARCH = $(shell $(TOPDIR)/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/rpmarch.guess module $(TOPDIR))

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
	mkinstalldirs	\
	rpmarch.guess


#
# Support files
#

$(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm: $(TOPDIR)/vendor/common/oracleasm-support.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" "$(TOPDIR)/vendor/common/oracleasm-support.spec"

support_srpm: $(TOPDIR)/oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm

support_rpm: support_srpm
	$(RPMBUILD) --rebuild $(TOOLSARCH) "oracleasm-support-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


#
# SLES 9
#

$(TOPDIR)/vendor/suse/oracleasm-2.6.5-%.spec: $(TOPDIR)/vendor/suse/oracleasm-2.6.5.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-2.6.5-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/^%define sver.*%{generic}$$/%define sver		'$${SPECVER}'/' < $< > $@

sles9_%_srpm: dist $(TOPDIR)/vendor/suse/oracleasm-2.6.5-%.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" $(TOPDIR)/vendor/suse/oracleasm-2.6.5-$(patsubst sles9_%_srpm,%,$@).spec

sles9_%_rpm: sles9_%_srpm
	$(RPMBUILD) --rebuild $(MODULEARCH) "oracleasm-2.6.5-$(patsubst sles9_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


#
# Include this at the very end of the Makefile
#
include $(TOPDIR)/Postamble.make
