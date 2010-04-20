#
# Header files
#

# This Vendor.make expects TOOLSARCH set by someone including it
# It also expects PKG_VERSION, INCLUDE_REQUIRES, and VENDOR_EXTENSION to be
# set.

include $(TOPDIR)/vendor/common/subst.make
include $(TOPDIR)/vendor/common/gmsl

RPMBUILD = $(shell /usr/bin/which rpmbuild 2>/dev/null || /usr/bin/which rpm 2>/dev/null || echo /bin/false)
RPM_TOPDIR = `pwd`

ifndef VENDOR_EXTENSION
VENDOR_EXTENSION = common
endif

DPV=$(DIST_VERSION)-$(PKG_VERSION).$(VENDOR_EXTENSION)
HEADER_SPEC_IN=$(TOPDIR)/vendor/common/oracleasm-headers.spec-generic
HEADER_SPEC_OUT=$(TOPDIR)/vendor/common/oracleasm-headers.spec

$(TOPDIR)/oracleasm-headers-$(DPV).src.rpm: $(HEADER_SPEC_IN)
	$(spec_subst) > $(HEADER_SPEC_OUT)
	$(RPMBUILD) -bs --define "_sourcedir $(RPM_TOPDIR)" \
		--define "_srcrpmdir $(RPM_TOPDIR)" "$(HEADER_SPEC_OUT)"

headers_srpm: $(TOPDIR)/oracleasm-headers-$(DPV).src.rpm

headers_rpm: headers_srpm
	$(RPMBUILD) --rebuild $(TOOLSARCH) "oracleasm-headers-$(DPV).src.rpm"

#
# Build a package out of the compiled source tree
#

BIN_SPEC_IN=$(TOPDIR)/vendor/common/binrpm.spec-generic

$(TOPDIR)/vendor/common/binrpm.spec:\
	KERNEL_VERSION=$(KERNELVER)

$(TOPDIR)/vendor/common/binrpm.spec: $(BIN_SPEC_IN) dist
	$(spec_subst) > $@

binrpm-pkg: $(TOPDIR)/vendor/common/binrpm.spec
	$(RPMBUILD) --define "_builddir $(TOPDIR)" --target $(KERNELARCH) -bb $<

