/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmcompat32.h - Oracle Automatic Storage Management library ID macros
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _ASMCOMPAT32_H
#define _ASMCOMPAT32_H

/*
 * This is ugly.  SIZEOF_UNSIGNED_LONG comes from autoconf.
 * Do you have a better way?  I chose not to hand-cook an autoconf
 * test because I'm lazy and it doesn't seem significantly better.
 */
#ifndef BITS_PER_LONG
# if SIZEOF_UNSIGNED_LONG == 4
#  define BITS_PER_LONG 32
# else
#  if SIZEOF_UNSIGNED_LONG == 8
#   define BITS_PER_LONG 64
#  else
#   error Unknown size of unsigned long (SIZEOF_UNSIGNED_LONG)
#  endif  /* SIZEOF_UNSIGNED_LONG == 8 */
# endif  /* SIZEOF_UNSIGNED_LONG == 4 */
#endif  /* BITS_PER_LONG */

/*
 * Handle the ID sizes
 */
#define HIGH_UB4(_ub8)          ((unsigned long)(((_ub8) >> 32) & 0xFFFFFFFFULL))
#define LOW_UB4(_ub8)           ((unsigned long)((_ub8) & 0xFFFFFFFFULL))


/*
 * Handle CONFIG_COMPAT for 32->64bit.  UGLY in 2.4.
 * This is REALLY UGLY.  I hate you all.
 */
#if defined(__KERNEL__)
# if BITS_PER_LONG == 64
#  if defined(CONFIG_COMPAT)
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#    if defined(__ia64__) || defined(__powerpc64__)
#     undef CONFIG_COMPAT
#    else
#     include <asm/ioctl32.h>
#    endif  /* !__ia64__  && !__powerpc64__ */
#   else
#    include <linux/ioctl32.h>
#   endif  /* LINUX_VERSION_CODE < 2.6.0 */
#  endif  /* CONFIG_COMPAT */
#  if !defined(CONFIG_COMPAT) && \
	(LINUX_VERSION_CODE > KERNEL_VERSION(2,4,18))
#   define CONFIG_COMPAT 1
#   if defined(__x86_64__)
#    include <asm/ioctl32.h>
#   else
#    if defined(__powerpc64__)
extern int register_ioctl32_conversion(unsigned int cmd, int (*handler)(unsigned int, unsigned int, unsigned long, struct file *));
extern int unregister_ioctl32_conversion(unsigned int cmd);
#    else
#     undef CONFIG_COMPAT
#    endif  /* __powerpc64__ */
#   endif  /* __x86_64__ */
#  endif  /* CONFIG_COMPAT */
# endif  /* BITS_PER_LONG == 64 */
#endif  /* __KERNEL__ */

#endif  /* _ASMCOMPAT32_H */

