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
#include "osmerror.h"
#include "asm/osmstructures.h"
#include "linux/osmabi.h"
#include "linux/osmdisk.h"


/*
 * Defines
 */
#define OSMLIB_NAME "OSM Library - Generic Linux"
#define OSMLIB_MAJOR 0
#define OSMLIB_MINOR 7
#define OSMLIB_MICRO 0


/*
 * Functions
 */


uword osm_version(ub4 *version, osm_iid *iid, oratext *name,
                  uword len, uword *interface_mask)
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

    *interface_mask = OSM_IO | OSM_UDID | OSM_FGROUP | OSM_OSNAME;

    ret = OSM_INIT_SUCCESS;

out:
    return ret;
}  /* osm_version() */


/* osm_erc osm_init(osm_iid iid, ub4 app, osm_ctx *ctxp) */
osm_erc osm_init(osm_iid iid, osm_ctx *ctxp)
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

    priv->iid = iid;
#if 0 /* wither app? */
    priv->app = app;
#endif
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
    osm_name_private *pname = (osm_name_private *)name;
    osm_erc err;
    glob_t *globbuf;
    char *path = NULL;
    int rc, fd, len, to_clear;
    unsigned long size;
    struct stat stat_buf;
    struct osm_disk_query dq;

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
            if (!rc && S_ISBLK(stat_buf.st_mode)) 
            {
                dq.dq_rdev = stat_buf.st_rdev;
                dq.dq_maxio = 0;
                rc = ioctl(priv->fd, OSMIOC_QUERYDISK, &dq);
                if (!rc)
                {
                    /* Use size temporary for ub4 platforms */
                    rc = ioctl(fd, BLKGETSIZE, &size);
                    if (!rc)
                    {
                        pname->size_osm_name = size;
                        rc = ioctl(fd, BLKSSZGET,
                                   &(pname->blksz_osm_name));
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
    memmove(pname->path_osm_name,
            globbuf->gl_pathv[priv->discover_index], len);
    pname->path_osm_name[len] = '\0';
    pname->label_osm_name[0] = '\0';
    pname->interface_osm_name = (OSM_IO | OSM_OSNAME);
    pname->reserved_osm_name_low = stat_buf.st_rdev;
    pname->maxio_osm_name = dq.dq_maxio / pname->blksz_osm_name;

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
        memset(pname, 0, sizeof(*pname));

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_fetch() */


osm_erc osm_open(osm_ctx ctx, osm_name *name, osm_handle *hand)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_name_private *pname = (osm_name_private *)name;
    osm_erc err;
    int rc;
    unsigned long handle;

    err = OSM_ERR_INVAL;
    if (!priv || !pname || !hand ||
        !pname->blksz_osm_name || !(pname->interface_osm_name & OSM_IO))
        goto out;

    /* FIXME: need to handle fatal errors when the kernel can tell */
    err = OSM_ERR_PERM;
    handle = pname->reserved_osm_name_low;
    rc = ioctl(priv->fd, OSMIOC_OPENDISK, &handle);
    if (rc)
        goto out;

    err = OSM_ERR_NOMEM;
    if (!handle)
        goto out;

    *hand = handle;

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_open() */


osm_erc osm_close(osm_ctx ctx, osm_handle hand)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_erc err;
    int rc;
    unsigned long handle;

    err = OSM_ERR_INVAL;
    if (!priv || !hand)
        goto out;

    /* FIXME: need to handle fatal errors when the kernel can tell */
    handle = REAL_HANDLE(hand);
    err = OSM_ERR_INVAL;
    rc = ioctl(priv->fd, OSMIOC_CLOSEDISK, &handle);
    if (rc)
        goto out;

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_close() */


osm_erc osm_io(osm_ctx ctx,
               osm_ioc *requests[], uword reqlen,
               osm_ioc *waitreqs[], uword waitlen,
               osm_ioc *completions[], uword complen,
               ub4 intr, ub4 timeout, uword *statusp)
{
    osm_erc err;
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    struct osmio io;
    struct timespec ts;
    int rc;

    if (!priv)
        return OSM_ERR_INVAL;

    /* Clearing statusp */
    *statusp = 0;

    /* io.handle = what? */
    io.requests = requests;
    io.reqlen = reqlen;
    io.waitreqs = waitreqs;
    io.waitlen = waitlen;
    io.completions = completions;
    io.complen = complen;
    io.intr = intr;
    if (timeout == OSM_WAIT)
        io.timeout = NULL;
    else
    {
        if (timeout == OSM_NOWAIT)
            ts.tv_sec = ts.tv_nsec = 0;
        else
        {
            ts.tv_sec = timeout / 1000000;
            ts.tv_nsec = (timeout % 1000000) * 1000;
        }
        io.timeout = &ts;
    }
    io.statusp = statusp;

    rc = ioctl(priv->fd, OSMIOC_IODISK, &io);

    if (rc)
    {
        switch (errno)
        {
            default:
                fprintf(stderr, "OSM: Invalid error of %d!\n", errno);
                err = OSM_ERR_INVAL;
                break;
    
            case EFAULT:
                err = OSM_ERR_FAULT;
                break;
    
            case EIO:
                err = OSM_ERR_IO;
                break;
    
            case ENODEV:
                err = OSM_ERR_NODEV;
                break;
    
            case ENOMEM:
                err = OSM_ERR_NOMEM;
                break;
    
            case EINVAL:
                err = OSM_ERR_INVAL;
                break;
                
            case EINTR:
                err = OSM_ERR_NONE;
                *statusp |= OSM_IO_POSTED;
                break;

            case ETIMEDOUT:
                err = OSM_ERR_NONE;
                *statusp |= OSM_IO_TIMEOUT;
                break;
	}
    }
    else
        err = OSM_ERR_NONE;

    return err;
}  /* osm_io() */


/*
 * Error code strings must be kept in sync with osmerror.h
 */
const oratext *osm_errstr_pos[] =
{
    "No error",                         /* OSM_ERR_NONE */
    "Operation not permitted",          /* OSM_ERR_PERM */
    "Out of memory",                    /* OSM_ERR_NOMEM */
    "I/O Error",                        /* OSM_ERR_IO */
};

const oratext *osm_errstr_neg[] = 
{
    "No error",                         /* OSM_ERR_NONE */
    "Invalid argument",                 /* OSM_ERR_INVAL */
    "Invalid IID",                      /* OSM_ERR_BADIID */
    "No such device",                   /* OSM_ERR_NODEV */
    "Invalid address",                  /* OSM_ERR_FAULT */
};


osm_erc osm_error(osm_ctx ctx, osm_erc errcode,
                  oratext *errbuf, uword eblen)
{
    osm_erc err;

    err = OSM_ERR_INVAL;
    if (!errbuf || (eblen < 1))
        goto out;

    if (errcode >= OSM_ERR_NONE)
        snprintf(errbuf, eblen, "%s", osm_errstr_pos[errcode]);
    else
        snprintf(errbuf, eblen, "%s", osm_errstr_neg[-errcode]);

    err = OSM_ERR_NONE;

out:
    return err;
}  /* osm_error() */


osm_erc osm_ioerror(osm_ctx ctx, osm_ioc *ioc,
                    oratext *errbuf, uword eblen)
{
    osm_erc errcode;

    if (!ioc)
        return OSM_ERR_INVAL;

    errcode = ioc->error_osm_ioc;
    fprintf(stdout, "Error code of %d\n", errcode);

    return osm_error(ctx, errcode, errbuf, eblen);
}  /* osm_ioerror() */


osm_erc osm_iowarn(osm_ctx ctx, osm_ioc *ioc,
                   oratext *wrnbuf, uword wblen)
{
    return osm_ioerror(ctx, ioc, wrnbuf, wblen);
}  /* osm_iowarn() */


/* Stub implementation, no real cancel */
osm_erc osm_cancel(osm_ctx ctx, osm_ioc *ioc)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;

    if (!priv || !ioc || (ioc->status_osm_ioc & OSM_FREE))
        return OSM_ERR_INVAL;

    ioc->status_osm_ioc |= OSM_CANCELLED;

    return 0;
}  /* osm_cancel() */


void osm_posted(osm_ctx ctx)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;

    if (!priv)
        return;

    priv->posted++;
}  /* osm_posted() */

#if 1
/* Debugging */
void osm_dump(osm_ctx ctx)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;

    if (!priv)
        return;

    ioctl(priv->fd, OSMIOC_DUMP, 0);
}  /* osm_dump() */
#endif
