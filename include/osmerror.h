
/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmerror.h - Oracle Storage Manager internal error header
    
  DESCRIPTION
  
  This file defines the error codes and descriptions for the OSM
  library on Linux.
*/


#ifndef _OSMERROR_H
#define _OSMERROR_H

/*
 * Error codes.  Positive means runtime error, negative means software
 * error.  See osmlib.c for the description strings.
 */
enum _OsmErrors
{
    OSM_ERR_BADIID      = -2,   /* Invalid IID */
    OSM_ERR_INVAL       = -1,   /* Invalid argument */
    OSM_ERR_NONE        = 0,    /* No error */
    OSM_ERR_PERM	= 1,	/* Operation not permitted */
    OSM_ERR_NOMEM	= 2,	/* Out of memory */
};

#endif  /* _OSMERROR_H */
