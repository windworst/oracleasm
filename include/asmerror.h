/*
 * NAME
 *	asmerror.h - Oracle ASM library internal error header.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This file contains the internal error code mappings for the
 *      Oracle Automatic Storage Managment userspace library.
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
