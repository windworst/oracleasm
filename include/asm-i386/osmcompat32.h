/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmcompat32.h - Oracle Storage Manager ID macros
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _OSMCOMPAT32_H
#define _OSMCOMPAT32_H

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

#endif  /* _OSMCOMPAT32_H */

