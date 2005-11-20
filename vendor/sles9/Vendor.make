#
# SLES 9
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/sles9/rpmarch.guess module $(TOPDIR))


$(TOPDIR)/vendor/sles9/oracleasm-2.6.5-%.spec: $(TOPDIR)/vendor/sles9/oracleasm-2.6.5.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-2.6.5-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		sed -e 's/^%define sver.*%{generic}$$/%define sver		'$${SPECVER}'/' < $< > $@

sles9_%_srpm: dist $(TOPDIR)/vendor/sles9/oracleasm-2.6.5-%.spec
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" --define "_srcrpmdir $(RPM_TOPDIR)" $(TOPDIR)/vendor/sles9/oracleasm-2.6.5-$(patsubst sles9_%_srpm,%,$@).spec

sles9_%_rpm: sles9_%_srpm
	$(RPMBUILD) --rebuild $(MODULEARCH) "oracleasm-2.6.5-$(patsubst sles9_%_rpm,%,$@)-$(DIST_VERSION)-$(RPM_VERSION).src.rpm"


include $(TOPDIR)/vendor/common/Vendor.make
