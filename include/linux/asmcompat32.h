/*
 * NAME
 *	asmcompat32.h - ASM library 32<->64bit compatibilty support.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This file contains helpers for supporting 32bit, 64bit, and 
 *      32bit-on-64bit implementations of the Oracle Automatic Storage
 *      Management library.
 *
 * MODIFIED   (YYYY/MM/DD)
 *      2004/01/02 - Joel Becker <joel.becker@oracle.com>
 *              Initial LGPL header.
 *
 * Copyright (c) 2002-2004 Oracle Corporation.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License, version 2 as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have recieved a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */


/*
 * This file is an internal header to the asmlib implementation on
 * Linux.  This file presumes the definitions in asmlib.h and
 * oratypes.h.
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
#   ifndef CONFIG_UNITEDLINUX_KERNEL  /* ARGH! */
extern long sys_ioctl(unsigned int fd, unsigned int cmd,
                      unsigned long arg);
#   endif  /* CONFIG_UNITEDLINUX_KERNEL */
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

