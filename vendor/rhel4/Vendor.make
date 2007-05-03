#
# RHEL 4
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/rhel4/rpmarch.guess module $(TOPDIR))
VENDOR_EXTENSION = el4

$(TOPDIR)/vendor/rhel4/oracleasm-%.spec: $(TOPDIR)/vendor/rhel4/oracleasm.spec-generic
	SPECVER="$@"; \
		SPECVER="$${SPECVER#*oracleasm-}"; \
		SPECVER="$${SPECVER%.spec}"; \
		LARGEVER="$${SPECVER#2.6.9-}"; \
		LARGEVER="$${LARGEVER%%.*}"; \
		if [ "$${LARGEVER}" -lt 34 ]; \
		then \
			LARGESMP=0; \
		else \
			LARGESMP=1; \
		fi; \
		sed -e 's/@@KVER@@/'$${SPECVER}'/' -e 's/@@PKG_VERSION@@/'$(PKG_VERSION)'/' -e 's/@@LARGESMP@@/'$${LARGESMP}'/' < $< > $@

rhel4_%_srpm: dist $(TOPDIR)/vendor/rhel4/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/rhel4/oracleasm-$(patsubst rhel4_%_srpm,%,$@).spec

rhel4_%_rpm: rhel4_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst rhel4_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).$(VENDOR_EXTENSION).src.rpm"


# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-kernheaders
include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/rhel4/kernel.guess targets) headers_rpm
