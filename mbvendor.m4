AC_DEFUN([MB_VENDOR],
  [AC_MSG_CHECKING([for vendor])
   AC_ARG_WITH(vendor, [  --with-vendor=VENDOR    Vendor to tailor build defaults and packages to [common]],[
     mb_vendor="$withval"
     if test -x "vendor/${mb_vendor}/vendor.guess"; then
       if "vendor/${mb_vendor}/vendor.guess" >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
         AC_MSG_RESULT([$mb_vendor])
       else
         AC_MSG_RESULT([not found])
         AC_MSG_ERROR([Vendor $mb_vendor not detected])
       fi
     else
       AC_MSG_RESULT([not supported])
       AC_MSG_ERROR([Vendor $mb_vendor not supported])
     fi
  ], [
    mb_vendor=`./vendor.guess 2>&AS_MESSAGE_LOG_FD`
    if test -z "$mb_vendor"; then
       AC_MSG_RESULT([not found])
    else
       AC_MSG_RESULT([$mb_vendor])
    fi
  ])
])  # MB_VENDOR
