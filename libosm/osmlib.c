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
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* BLKGETSIZE etc */
#include <linux/fs.h>

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
    priv->discover_cache = NULL;

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


/* First cut: glob() string.  NULL is invalid; empty string is valid. */
osm_erc osm_discover(osm_ctx ctx, oratext *setdesc)
{
    osm_erc err;
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    glob_t *globbuf;
    int rc;

    err = OSM_ERR_INVAL;
    if (!priv)
        goto out;
    if (!setdesc)
        goto out;

    /* Orahack - someone decided "" == "*" */
    if (*setdesc == '\0')
        setdesc = "*";

    err = OSM_ERR_NOMEM;
    globbuf = (glob_t *)malloc(sizeof(*globbuf));
    if (!globbuf)
        goto out;

    rc = glob(setdesc, 0, NULL, globbuf);

    if (rc)
    {
        free(globbuf);
        if (rc == GLOB_NOSPACE)
            goto out;
    }
    else
    {
        priv->discover_cache = globbuf;
        priv->discover_index = 0;
    }
    
    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_discover() */


osm_erc osm_fetch(osm_ctx ctx, osm_name *name)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_erc err;
    glob_t *globbuf;
    char *path;
    int rc, fd, len, to_clear;
    struct stat stat_buf;

    err = OSM_ERR_INVAL;
    if (!priv)
        goto out;

    to_clear = 1;
    globbuf = (glob_t *)priv->discover_cache;
    if (!globbuf)
        goto clear_name;

    while (priv->discover_index < globbuf->gl_pathc)
    {
        path = globbuf->gl_pathv[priv->discover_index];

        fd = open(path, O_RDWR);
        if (fd >= 0)
        {
            rc = fstat(fd, &stat_buf);
            if (!rc) 
            {
                rc = ioctl(priv->fd, OSMIOC_ISDISK, &(stat_buf.st_rdev));
                if (!rc)
                {
                    rc = ioctl(fd, BLKGETSIZE, &(name->size_osm_name));
                    if (!rc)
                    {
                        rc = ioctl(fd, BLKSSZGET,
                                   &(name->blksz_osm_name));
                        if (!rc)
                        {
                            close(fd);
                            break;
                        }
                    }
                }
            }
            close(fd);
        }

        priv->discover_index++;
    }
    if (priv->discover_index >= globbuf->gl_pathc)
        goto end_glob;

    to_clear = 0;

    /* strncpy sucks */
    len = strlen(path);
    if (len >= OSM_MAXPATH)
        len = OSM_MAXPATH -1;
    memmove(name->path_osm_name,
            globbuf->gl_pathv[priv->discover_index], len);
    name->path_osm_name[len] = '\0';
    name->label_osm_name[0] = '\0';
    name->interface_osm_name = (OSM_IO | OSM_OSNAME | OSM_SIZE);


    priv->discover_index++;
end_glob:
    if (priv->discover_index >= globbuf->gl_pathc)
    {
        globfree(globbuf);
        free(globbuf);
        priv->discover_cache = NULL;
    }

clear_name:
    if (to_clear)
        memset(name, 0, sizeof(*name));

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_fetch() */


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
