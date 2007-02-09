#
# FC6
#

TOOLSARCH = $(shell $(TOPDIR)/vendor/fc6/rpmarch.guess tools $(TOPDIR))
MODULEARCH = $(shell $(TOPDIR)/vendor/fc6/rpmarch.guess module $(TOPDIR))

$(TOPDIR)/vendor/fc6/oracleasm-%.spec: $(TOPDIR)/vendor/fc6/oracleasm.spec-generic
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

fc6_%_srpm: dist $(TOPDIR)/vendor/fc6/oracleasm-%.spec
	rpmbuild -bs --define "_sourcedir $(TOPDIR)" --define "_srcrpmdir $(TOPDIR)" $(TOPDIR)/vendor/fc6/oracleasm-$(patsubst fc6_%_srpm,%,$@).spec

fc6_%_rpm: fc6_%_srpm
	rpmbuild --rebuild $(MODULEARCH) "oracleasm-$(patsubst fc6_%_rpm,%,$@)-$(DIST_VERSION)-$(PKG_VERSION).src.rpm"


# Package required for /usr/include/linux
INCLUDE_REQUIRES = glibc-kernheaders
include $(TOPDIR)/vendor/common/Vendor.make

packages: $(shell $(TOPDIR)/vendor/fc6/kernel.guess targets) headers_rpm
