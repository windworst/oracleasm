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

#define _LARGEFILE64_SOURCE

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
#include <linux/types.h>
#include <linux/fs.h>

#include "oratypes.h"
#include "osmlib.h"
#include "osmerror.h"
#include "linux/osmcompat32.h"
#include "linux/osmabi.h"
#include "linux/osmdisk.h"


/*
 * Defines
 */
#define OSMLIB_NAME     "OSM Library - Generic Linux"
#define DEVASM          "/dev/oracleasm"



/*
 * Typedefs
 */
typedef struct _osm_ctx_private osm_ctx_private;



/*
 * Structures
 */


/*
 * Private context pointer
 *
 * This is the internal representation of a context.  It will be casted
 * from the osm_ctx exposed to the user of osmlib.
 */
struct _osm_ctx_private
{
    osm_iid iid;
    int fd;
    int discover_index;
    void *discover_cache;
};



/*
 * Functions
 */


uword osm_version(ub4 *version, osm_iid *iid, oratext *name,
                  uword len, uword *interface_mask)
{
    uword ret;
    int fd, rc;
    struct oracleasm_get_iid new_iid;

    if (len)
        snprintf(name, len, "%s, version %s", OSMLIB_NAME, VERSION);

    ret = OSM_INIT_VERSION;
    if (*version & OSM_API_V1)
        *version = OSM_API_V1;
    else
        goto out;

    /*
     * The initial assumption is that root has properly loaded the
     * osm module and mounted /dev/oracleasm.  We might have osmlib do
     * this trick later.
     */
    ret = OSM_INIT_INSTALL;
    fd = open(DEVASM, O_RDONLY);
    if (fd < 0)
        goto out;

    new_iid.gi_version = ASM_ABI_VERSION;
    rc = ioctl(fd, ASMIOC_GETIID, &new_iid);
    close(fd);
    if (rc)
        goto out;

    ret = OSM_INIT_OTHER;
        *iid = (osm_iid)new_iid.gi_iid;

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
    struct oracleasm_get_iid real_iid;
    int fd, rc;
    char *osm_file;

    err = ASM_ERR_INVAL;
    if (*ctxp)
        goto out;

    err = ASM_ERR_PERM;
    fd = open(DEVASM, O_RDONLY);
    if (fd < 0)
        goto out;

    real_iid.gi_iid = iid;
    real_iid.gi_version = ASM_ABI_VERSION;
    rc = ioctl(fd, ASMIOC_CHECKIID, &real_iid);
    close(fd);
    if (rc)
        goto out;

    /* real_iid will be set to zero if it was invalid */
    err = ASM_ERR_BADIID;
    if (!real_iid.gi_iid)
        goto out;

    err = ASM_ERR_NOMEM;
    priv = (osm_ctx_private *)malloc(sizeof(*priv));
    if (!priv)
        goto out;

    /* 16 chars for 64 bits + 1 for \0 */
    osm_file = (char *)malloc(sizeof(char) * (strlen(DEVASM) + 17));
    if (!osm_file)
        goto out_free_ctx;

    sprintf(osm_file, "%s/%.8lX%.8lX", DEVASM,
            HIGH_UB4(real_iid.gi_iid),
            LOW_UB4(real_iid.gi_iid));

    err = ASM_ERR_PERM;
    priv->fd = open(osm_file, O_RDWR | O_CREAT, 0700);
    free(osm_file);
    if (priv->fd < 0)
        goto out_free_ctx;

    priv->iid = iid;
    priv->discover_cache = NULL;

    *ctxp = (osm_ctx *)priv;
    err = ASM_ERR_NONE;
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
        return ASM_ERR_INVAL;

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

    err = ASM_ERR_INVAL;
    if (!priv)
        goto out;
    if (!setdesc)
        goto out;

    /* Orahack - someone decided "" == "*" */
    if (*setdesc == '\0')
        setdesc = "*";

    err = ASM_ERR_NOMEM;
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
    
    err = ASM_ERR_NONE;

out:
    return err;
}  /* osm_discover() */


osm_erc osm_fetch(osm_ctx ctx, osm_name *name)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_erc err;
    glob_t *globbuf;
    char *path = NULL;
    int rc, fd, len, to_clear;
    struct stat stat_buf;
    struct oracleasm_disk_query dq;

    err = ASM_ERR_INVAL;
    if (!priv)
        goto out;

    to_clear = 1;
    globbuf = (glob_t *)priv->discover_cache;
    if (!globbuf)
        goto clear_name;

    while (priv->discover_index < globbuf->gl_pathc)
    {
        path = globbuf->gl_pathv[priv->discover_index];

        fd = open(path, O_RDONLY);
        if (fd >= 0)
        {
            rc = fstat(fd, &stat_buf);
            if (!rc && S_ISBLK(stat_buf.st_mode)) 
            {
                dq.dq_rdev = (__u64)stat_buf.st_rdev;
                dq.dq_maxio = 0;
                rc = ioctl(priv->fd, ASMIOC_QUERYDISK, &dq);
                if (!rc)
                {
                    name->size_osm_name = lseek64(fd, 0ULL, SEEK_END);
                    if (name->size_osm_name < SB8MAXVAL)
                    {
                        rc = ioctl(fd, BLKSSZGET,
                                   &(name->blksz_osm_name));
                        if (!rc)
                        {
                            name->size_osm_name /= name->blksz_osm_name;
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
        len = OSM_MAXPATH - 1;
    memmove(name->path_osm_name,
            globbuf->gl_pathv[priv->discover_index], len);
    name->path_osm_name[len] = '\0';
    name->label_osm_name[0] = '\0';
    name->interface_osm_name = (OSM_IO | OSM_OSNAME);
    name->reserved_osm_name = dq.dq_rdev;
    name->maxio_osm_name = dq.dq_maxio / name->blksz_osm_name;

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

    err = ASM_ERR_NONE;

out:
    return err;
}  /* osm_fetch() */


osm_erc osm_open(osm_ctx ctx, osm_name *name, osm_handle *hand)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_erc err;
    int rc, fd;
    struct oracleasm_disk_query handle;
    struct stat stat_buf;

    err = ASM_ERR_INVAL;
    if (!priv || !name || !hand ||
        !name->blksz_osm_name || !(name->interface_osm_name & OSM_IO))
        goto out;


    fd = open(name->path_osm_name, O_RDONLY);
    if (fd < 0)
    {
        switch (errno)
        {
            case EPERM:
            case EACCES:
            case EROFS:
            case EMFILE:
            case ENFILE:
                err = ASM_ERR_PERM;
                break;

            case ENOMEM:
                err = ASM_ERR_NOMEM;
                break;

            case EINVAL:
                err = ASM_ERR_INVAL;
                break;

            default:
                err = ASM_ERR_NODEV;
                break;
        }

        goto out;
    }

    err = ASM_ERR_INVAL;
    rc = fstat(fd, &stat_buf);
    if (rc)
        goto out_close;
    err = ASM_ERR_NODEV;
    if (!S_ISBLK(stat_buf.st_mode) ||
        ((__u64)stat_buf.st_rdev != name->reserved_osm_name)) 
        goto out_close;

    /* FIXME: need to handle fatal errors when the kernel can tell */
    err = ASM_ERR_PERM;
    handle.dq_rdev = name->reserved_osm_name;
    rc = ioctl(priv->fd, ASMIOC_OPENDISK, &handle);
    if (rc)
        goto out_close;

    err = ASM_ERR_NOMEM;
    if (!handle.dq_rdev)
        goto out_close;

    *hand = handle.dq_rdev;

    err = ASM_ERR_NONE;

out_close:
    close(fd);

out:
    return err;
}  /* osm_open() */


osm_erc osm_close(osm_ctx ctx, osm_handle hand)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    osm_erc err;
    int rc;
    struct oracleasm_disk_query handle = {hand, 0};

    err = ASM_ERR_INVAL;
    if (!priv || !hand)
        goto out;

    /* FIXME: need to handle fatal errors when the kernel can tell */
    err = ASM_ERR_INVAL;
    rc = ioctl(priv->fd, ASMIOC_CLOSEDISK, &handle);
    if (rc)
        goto out;

    err = ASM_ERR_NONE;

out:
    return err;
}  /* osm_close() */


osm_erc osm_io(osm_ctx ctx,
               osm_ioc *requests[], uword reqlen,
               osm_ioc *waitreqs[], uword waitlen,
               osm_ioc *completions[], uword complen,
               ub4 timeout, uword *statusp)
{
    osm_erc err;
    osm_ctx_private *priv = (osm_ctx_private *)ctx;
    struct oracleasm_io io;
    struct timespec ts;
    int rc;

    if (!priv)
        return ASM_ERR_INVAL;

    /* Clearing statusp */
    *statusp = 0;

    /* io.handle = what? */
    io.io_requests = (__u64)(unsigned long)requests;
    io.io_reqlen = reqlen;
    io.io_waitreqs = (__u64)(unsigned long)waitreqs;
    io.io_waitlen = waitlen;
    io.io_completions = (__u64)(unsigned long)completions;
    io.io_complen = complen;
    if (timeout == OSM_WAIT)
        io.io_timeout = (__u64)(unsigned long)NULL;
    else if (waitlen)
        return ASM_ERR_INVAL;
    else
    {
        if (timeout == OSM_NOWAIT)
            ts.tv_sec = ts.tv_nsec = 0;
        else
        {
            ts.tv_sec = timeout / 1000000;
            ts.tv_nsec = (timeout % 1000000) * 1000;
        }
        io.io_timeout = (__u64)(unsigned long)&ts;
    }
    io.io_statusp = (__u64)(unsigned long)statusp;

    rc = ioctl(priv->fd, ASMIOC_IODISK, &io);

    if (rc)
    {
        switch (errno)
        {
            default:
                fprintf(stderr, "OSM: Invalid error of %d!\n", errno);
                err = ASM_ERR_INVAL;
                break;
    
            case EFAULT:
                err = ASM_ERR_FAULT;
                break;
    
            case EIO:
                err = ASM_ERR_IO;
                break;
    
            case ENODEV:
                err = ASM_ERR_NODEV;
                break;
    
            case ENOMEM:
                err = ASM_ERR_NOMEM;
                break;
    
            case EINVAL:
                err = ASM_ERR_INVAL;
                break;
                
            case EINTR:
                err = ASM_ERR_NONE;
                *statusp |= OSM_IO_POSTED;
                break;

            case ETIMEDOUT:
                err = ASM_ERR_NONE;
                *statusp |= OSM_IO_TIMEOUT;
                break;
	}
    }
    else
        err = ASM_ERR_NONE;

    return err;
}  /* osm_io() */


/*
 * Error code strings must be kept in sync with osmerror.h
 */
const oratext *osm_errstr_pos[] =
{
    "No error",                         /* ASM_ERR_NONE */
    "Operation not permitted",          /* ASM_ERR_PERM */
    "Out of memory",                    /* ASM_ERR_NOMEM */
    "I/O Error",                        /* ASM_ERR_IO */
};

const oratext *osm_errstr_neg[] = 
{
    "No error",                         /* ASM_ERR_NONE */
    "Invalid argument",                 /* ASM_ERR_INVAL */
    "Invalid IID",                      /* ASM_ERR_BADIID */
    "No such device",                   /* ASM_ERR_NODEV */
    "Invalid address",                  /* ASM_ERR_FAULT */
};


osm_erc osm_error(osm_ctx ctx, osm_erc errcode,
                  oratext *errbuf, uword eblen)
{
    osm_erc err;

    err = ASM_ERR_INVAL;
    if (!errbuf || (eblen < 1))
        goto out;

    if (errcode >= ASM_ERR_NONE)
        snprintf(errbuf, eblen, "%s", osm_errstr_pos[errcode]);
    else
        snprintf(errbuf, eblen, "%s", osm_errstr_neg[-errcode]);

    err = ASM_ERR_NONE;

out:
    return err;
}  /* osm_error() */


osm_erc osm_ioerror(osm_ctx ctx, osm_ioc *ioc,
                    oratext *errbuf, uword eblen)
{
    osm_erc errcode;

    if (!ioc)
        return ASM_ERR_INVAL;

    errcode = ioc->error_osm_ioc;

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
        return ASM_ERR_INVAL;

    ioc->status_osm_ioc |= OSM_CANCELLED;

    return 0;
}  /* osm_cancel() */


#if 1
/* Debugging */
void osm_dump(osm_ctx ctx)
{
    osm_ctx_private *priv = (osm_ctx_private *)ctx;

    if (!priv)
        return;

    ioctl(priv->fd, ASMIOC_DUMP, 0);
}  /* osm_dump() */
#endif
