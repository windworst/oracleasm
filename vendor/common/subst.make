#
# Substitution helpers
#
# Written by Martin K. Petersen <martin.petersen@oracle.com>
#

#
# Available for substitution in spec files
#
SUBST_VARS := PKG_VERSION DIST_VERSION VENDOR_EXTENSION INCLUDE_REQUIRES \
	    KERNEL_VERSION KERNEL_REQUIRES LARGESMP XEN DEBUG

#
# Substitute @@VAR@@ from list above in $<
#
define spec_subst
sed $(foreach SUB, $(SUBST_VARS), $(shell echo "-e s/@@$(SUB)@@/$($(SUB))/")) < $<
endef

#
# Helpers for extracting kernel version information from a spec file
# name.  foo/bar/baz/kernel-2.6.32.9-0315.43.OLE.spec will yield:
#

# kernel
define get_name
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([A-Za-z0-9_+\.]+)-([A-Za-z0-9_+\.]+)/\1/;")
endef

# 2.6.32.9-0315.43.OLE
define get_version_release
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([A-Za-z0-9_+\.]+)-([A-Za-z0-9_+\.]+)/\2-\3/;")
endef

# 2.6.32.9
define get_version
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([A-Za-z0-9_+\.]+)-([A-Za-z0-9_+\.]+)/\2/;")
endef

# 2.6
define get_version_major
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([0-9]+\.[0-9]+)(\.[0-9\.]+)?-([A-Za-z0-9_+\.]+)/\2/;")
endef

# 32.9
define get_version_minor
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([0-9]+\.[0-9]+)(\.[0-9\.]+)?-([A-Za-z0-9_+\.]+)/\3/;" \
	-e "s/^\.//")
endef

# 0315.43.OLE
define get_release
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([A-Za-z0-9_+\.]+)-([A-Za-z0-9_+\.]+)/\3/;")
endef

# 0315
define get_release_major
$(shell echo $(notdir $@) | sed -re "s/\.spec//" -e \
	"s/(^[A-Za-z0-9_+\.\-]+)-([A-Za-z0-9_+\.]+)-([0-9]+).*/\3/;")
endef

