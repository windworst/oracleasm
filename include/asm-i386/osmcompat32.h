/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmids.h - Oracle Storage Manager ID macros
    
  DESCRIPTION
  
  This file is an internal header to the osmlib implementation on Linux.

  This file presumes the definitions in osmlib.h and oratypes.h
*/


#ifndef _OSMIDS_H
#define _OSMIDS_H

/*
 * Handle the ID sizes
 */
#define HIGH_UB4(_ub8)          ((unsigned long)(((_ub8) >> 32) & 0xFFFFFFFFULL))
#define LOW_UB4(_ub8)           ((unsigned long)((_ub8) & 0xFFFFFFFFULL))

#endif  /* _OSMIDS_H */
