dnl This version can take multiple include paths
dnl OCFS2_CHECK_KERNEL_INCLUDES(FEATURE, HEADER-LIST, INCLUDE-PATHS,
dnl                             [ACTION-IF-TRUE], [ACTION-IF-FALSE],
dnl                             [REGEX])
dnl If REGEX is not provided, FEATURE is assumed to be a string to
dnl match.  Unlike OCFS2_CHECK_KERNEL, includes must be the full include
dnl ("linux/fs.h", not "fs.h").
AC_DEFUN([OCFS2_CHECK_KERNEL_INCLUDES],
  [AC_MSG_CHECKING([for $1])
   kernel_check_regexp="m4_default([$6], [\<$1(])"

   kernel_check_headers=
   for kinclude in $3; do
     for kfile in $2; do
       if test -e "$kinclude/$kfile"; then
         kernel_check_headers="$kernel_check_headers $kinclude/$kfile"
       fi
     done
   done

   if test x"$kernel_check_headers" != "x" && \
     grep "$kernel_check_regexp" $kernel_check_headers >/dev/null 2>&1 ; then
     m4_default([$4], :)
     AC_MSG_RESULT(yes)
   else
     m4_default([$5], :)
     AC_MSG_RESULT(no)
   fi
])# OCFS2_CHECK_KERNEL_INCLUDES

AC_DEFUN([OCFS2_CHECK_KERNEL],
  [AC_MSG_CHECKING([for $1])
   kernel_check_regexp="m4_default([$5], [\<$1(])"

   kernel_check_headers=
   for kfile in $2; do
     kernel_check_headers="$kernel_check_headers $KERNELINC/linux/$kfile"
   done

   if grep "$kernel_check_regexp" $kernel_check_headers >/dev/null 2>&1 ; then
     m4_default([$3], :)
     AC_MSG_RESULT(yes)
   else
     m4_default([$4], :)
     AC_MSG_RESULT(no)
   fi
])# OCFS2_CHECK_KERNEL

AC_DEFUN([OCFS2_KERNEL_COMPAT],
  [OCFS2_CHECK_KERNEL($1, $2, , 
     [KAPI_COMPAT_HEADERS="$KAPI_COMPAT_HEADERS $1.h"])])
