/* Copyright (c) 2001, 2002, Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmlib.c - Oracle Storage Manager library code
    
  DESCRIPTION
  
  This is the main source file for the Linux implementation of osmlib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <oratypes.h>
#include <osmlib.h>
#include "osmprivate.h"
#include "osmerror.h"
#include "arch/osmstructures.h"



uword osm_version(ub4 *version, osm_iid *iid, oratext *name,
                  uword len, uword *interface, ub4 *maxio)
{
    uword ret;
    int fd, rc;
    long new_iid;

    if (len)
        snprintf(name, len, "%s, version %d.%d.%d", OSMLIB_NAME,
                 OSMLIB_MAJOR, OSMLIB_MINOR, OSMLIB_MICRO);


    ret = OSM_INIT_VERSION;
    if (*version & OSM_API_V1)
        *version = OSM_API_V1;
    else
        goto out;

    /*
     * The initial assumption is that root has properly loaded the
     * osm module and mounted /dev/osm.  We might have osmlib do this
     * trick later.
     */
    ret = OSM_INIT_INSTALL;
    fd = open("/dev/osm", O_RDONLY);
    if (fd < 0)
        goto out;

    rc = ioctl(fd, OSMIOC_GETIID, &new_iid);
    close(fd);
    if (rc)
        goto out;

    ret = OSM_INIT_OTHER;
        *iid = (osm_iid)new_iid;

    *interface = OSM_IO | OSM_UDID | OSM_FGROUP | OSM_SIZE |
        OSM_OSNAME | OSM_HARDEN;

    /* This should probably be returned from the kernel */
    *maxio = 1024 * 128;

    ret = OSM_INIT_SUCCESS;

out:
    return ret;
}  /* osm_version() */


osm_erc osm_init(osm_iid iid, ub4 app, osm_ctx *ctxp)
{
    osm_erc err;
    osm_ctx_private *priv;
    unsigned long real_iid = REAL_IID(iid);
    int fd, rc;
    char *osm_file;

    err = OSM_ERR_INVAL;
    if (*ctxp)
        goto out;
    /* Assume only "ORCL" apps */
    if (memcmp(&app, "ORCL", 4))
        goto out;

    err = OSM_ERR_PERM;
    fd = open("/dev/osm", O_RDONLY);
    if (fd < 0)
        goto out;

    rc = ioctl(fd, OSMIOC_CHECKIID, &real_iid);
    close(fd);
    if (rc)
        goto out;

    /* real_iid will be set to zero if it was invalid */
    err = OSM_ERR_BADIID;
    if (!real_iid)
        goto out;

    err = OSM_ERR_NOMEM;
    priv = (osm_ctx_private *)malloc(sizeof(*priv));
    if (!priv)
        goto out;

    /* 16 chars for 64 bits + 1 for \0 */
    osm_file = (char *)malloc(sizeof(char) * (strlen("/dev/osm") + 17));
    if (!osm_file)
        goto out_free_ctx;

    sprintf(osm_file, "/dev/osm/%.8lX%.8lX", 0L, real_iid);

    err = OSM_ERR_PERM;
    priv->fd = open(osm_file, O_RDWR | O_CREAT, 0700);
    free(osm_file);
    if (priv->fd < 0)
        goto out_free_ctx;

    priv->app = app;

    *ctxp = (osm_ctx *)priv;
    err = OSM_ERR_NONE;
    goto out;

out_free_ctx:
    free(priv);

out:
    return err;
}  /* osm_init() */


osm_erc osm_fini(osm_ctx ctx)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;

    if (!priv)
        return OSM_ERR_INVAL;

    if (priv->fd >= 0)
        close(priv->fd);

    free(priv);

    return 0;
}  /* osm_fini() */


/*
 * Error code strings must be kept in sync with osmerror.h
 */
const oratext *osm_errstr_pos[] =
{
    "No error",                         /* OSM_ERR_NONE */
    "Operation not permitted",          /* OSM_ERR_PERM */
    "Out of memory */",                 /* OSM_ERR_NOMEM */
};

const oratext *osm_errstr_neg[] = 
{
    "No error",                         /* OSM_ERR_NONE */
    "Invalid argument",                 /* OSM_ERR_INVAL */
    "Invalid IID",                      /* OSM_ERR_BADIID */
};


osm_erc osm_error(osm_ctx ctx, osm_erc errcode,
                  oratext *errbuf, uword eblen)
{
    osm_erc err;

    err = OSM_ERR_INVAL;
    if (!ctx || !errbuf || (eblen < 1))
        goto out;

    if (errcode >= OSM_ERR_NONE)
        snprintf(errbuf, eblen, "%s", osm_errstr_pos[errcode]);
    else
        snprintf(errbuf, eblen, "%s", osm_errstr_neg[errcode]);

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_error() */
