
/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmerror.h - Oracle Automatic Storage Management library internal error header
    
  DESCRIPTION
  
  This file defines the error codes and descriptions for the ASM
  library on Linux.
*/


#ifndef _ASMERROR_H
#define _ASMERROR_H

/*
 * Error codes.  Positive means runtime error, negative means software
 * error.  See asmlib.c for the description strings.
 */
enum _ASMErrors
{
    ASM_ERR_FAULT       = -4,   /* Invalid address */
    ASM_ERR_NODEV       = -3,   /* Invalid device */
    ASM_ERR_BADIID      = -2,   /* Invalid IID */
    ASM_ERR_INVAL       = -1,   /* Invalid argument */
    ASM_ERR_NONE        = 0,    /* No error */
    ASM_ERR_PERM	= 1,	/* Operation not permitted */
    ASM_ERR_NOMEM	= 2,	/* Out of memory */
    ASM_ERR_IO          = 3,    /* I/O error */
};

#endif  /* _ASMERROR_H */
