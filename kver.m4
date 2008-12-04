dnl OCFS2_CHECK_KVER(INCLUDE-PATH, VERSION-VARIABLE, [DESCRIPTION],
dnl                             [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([OCFS2_CHECK_KVER],
  [AC_LANG_PREPROC_REQUIRE()dnl
   AC_REQUIRE([AC_PROG_EGREP])dnl

   check_kver_DESC="$3"

   if test -n "$check_kver_DESC"; then
     check_kver_DESC=" $check_kver_DESC"
   fi

   if test -f "$1/linux/utsrelease.h"; then
     UTS_HEADER=utsrelease.h
   else
     UTS_HEADER=version.h
   fi

   check_kver_saved_CPPFLAGS="$CPPFLAGS"
   CPPFLAGS="-I$1 $CPPFLAGS -nostdinc"

   AC_MSG_CHECKING(for kernel$check_kver_DESC version)

   AC_LANG_CONFTEST([AC_LANG_SOURCE([
#include <linux/${UTS_HEADER}>

#ifdef UTS_RELEASE
check_kver_RESULT=UTS_RELEASE
#else
check_kver_RESULT=none
#endif
])])

   if (eval "$ac_cpp conftest.$ac_ext") 2>&AS_MESSAGE_LOG_FD |
     $EGREP "^check_kver_RESULT=" > conftest.kver 2>&1; then
     . ./conftest.kver
   else
     check_kver_RESULT='none'
   fi

   AC_MSG_RESULT($check_kver_RESULT)
   rm -f conftest*

   CPPFLAGS="$check_kver_saved_CPPFLAGS"

   if test "x$check_kver_RESULT" != "xnone"; then
     $2=$check_kver_RESULT
     m4_default([$4], :)
   else
     m4_default([$5],
       [AC_MSG_ERROR([Could not determine kernel$check_kver_DESC version.])])
   fi
])# OCFS2_CHECK_KVER