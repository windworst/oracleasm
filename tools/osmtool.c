/* Copyright (c) 2003 Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    osmtool.c - Oracle Storage Manager user tool
    
  DESCRIPTION
  
  This is the main source file for the OSM user tool.
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

#include <linux/types.h>

#include <oratypes.h>
#include <osmlib.h>
#include "osmprivate.h"

/*
 * Defines
 */
#define OSM_DISK_LABEL          "ORCLDISK"
#define OSM_DISK_LABEL_CLEAR    "ORCLCLRD"
#define OSM_DISK_LABEL_SIZE     8
#define OSM_DISK_LABEL_OFFSET   32


/*
 * Enums
 */
typedef enum
{
    OSMTOOL_CHECK,
    OSMTOOL_MARK,
    OSMTOOL_UNMARK
} OsmtoolOperation;

/*
 * Functions
 */


static void print_usage(int rc)
{
    FILE *output = rc ? stderr : stdout;

    fprintf(output, "Usage: osmtool <disk1> ...\n");
    exit(rc);
}  /* print_usage() */


static void print_version()
{
    fprintf(stderr, "osmtool version %d.%d.%d\n",
            OSMLIB_MAJOR, OSMLIB_MINOR, OSMLIB_MICRO);
    exit(0);
}

static int open_disk(const char *disk_name)
{
    int fd, rc;
    struct stat stat_buf;

    rc = stat(disk_name, &stat_buf);
    if (rc)
        return -errno;

    /* First check, try to avoid opening tapes */
    if (!S_ISBLK(stat_buf.st_mode))
        return -ENOTBLK;

    fd = open(disk_name, O_RDWR);
    if (fd < 0)
        return -errno;

    /* Second check, try not to write the wrong place */
    rc = fstat(fd, &stat_buf);
    if (rc)
        return -errno;
    
    if (!S_ISBLK(stat_buf.st_mode))
        return -ENOTBLK;

    return fd;
}  /* open_disk() */


static int check_disk(int fd)
{
    int rc, tot;
    char buf[OSM_DISK_LABEL_SIZE];  /* Why malloc? */

    if (fd < 0)
        return -EINVAL;

    rc = lseek(fd, OSM_DISK_LABEL_OFFSET, SEEK_SET);
    if (rc < 0)
        return -errno;

    tot = 0;
    while (tot < OSM_DISK_LABEL_SIZE)
    {
        rc = read(fd, buf + tot, OSM_DISK_LABEL_SIZE - tot);
        if (!rc)
            break;
        if (rc < 0)
        {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            else
                return -errno;
        }
        tot += rc;
    }

    if (tot < OSM_DISK_LABEL_SIZE)
        return -ENOSPC;

    /* Already an OSM disk */
    if (!memcmp(buf, OSM_DISK_LABEL, OSM_DISK_LABEL_SIZE))
        return -EEXIST;

    return 0;
}  /* check_disk() */


static int mark_disk(int fd)
{
    int rc, tot;
    char buf[OSM_DISK_LABEL_SIZE];

    if (fd < 0)
        return -EINVAL;

    rc = lseek(fd, OSM_DISK_LABEL_OFFSET, SEEK_SET);
    if (rc < 0)
        return -errno;

    memcpy(buf, OSM_DISK_LABEL, OSM_DISK_LABEL_SIZE);

    tot = 0;
    while (tot < OSM_DISK_LABEL_SIZE)
    {
        rc = write(fd, buf + tot, OSM_DISK_LABEL_SIZE - tot);
        if (!rc)
            break;
        if (rc < 0)
        {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            else
                return -errno;
        }
        tot += rc;
    }

    if (tot < OSM_DISK_LABEL_SIZE)
        return -ENOSPC;

    return 0;
}  /* mark_disk() */


static int unmark_disk(int fd)
{
    int rc, tot;
    char buf[OSM_DISK_LABEL_SIZE];

    if (fd < 0)
        return -EINVAL;

    rc = lseek(fd, OSM_DISK_LABEL_OFFSET, SEEK_SET);
    if (rc < 0)
        return -errno;

    memcpy(buf, OSM_DISK_LABEL_CLEAR, OSM_DISK_LABEL_SIZE);

    tot = 0;
    while (tot < OSM_DISK_LABEL_SIZE)
    {
        rc = write(fd, buf + tot, OSM_DISK_LABEL_SIZE - tot);
        if (!rc)
            break;
        if (rc < 0)
        {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            else
                return -errno;
        }
        tot += rc;
    }

    if (tot < OSM_DISK_LABEL_SIZE)
        return -ENOSPC;

    return 0;
}  /* mark_disk() */



/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int i, fd, rc;
    OsmtoolOperation op = OSMTOOL_CHECK;

    if (argc < 2)
        print_usage(1);

    if (!strcmp(argv[1], "-h") ||
        !strcmp(argv[1], "--help"))
        print_usage(0);

    if (!strcmp(argv[1], "-V") ||
        !strcmp(argv[1], "--version"))
        print_version();

    i = 2;
    if (!strcmp(argv[1], "-m"))
        op = OSMTOOL_MARK;
    else if (!strcmp(argv[1], "-u"))
        op = OSMTOOL_UNMARK;
    else if (!strcmp(argv[1], "-c"))
        op = OSMTOOL_CHECK;
    else if (argv[1][0] == '-')
        print_usage(1);
    else
        i = 1;

    for (; i < argc; i++)
    {
        fd = open_disk(argv[i]);
        if (fd < 0)
        {
            fprintf(stderr, "osmtool: Unable to open \"%s\": %s\n",
                    argv[i], strerror(-fd));
            continue;
        }

        rc = check_disk(fd);
        if (!rc)
        {
            if (op != OSMTOOL_MARK)
            {
                fprintf(stdout,
                        "osmtool: Disk \"%s\" is not an OSM disk\n",
                        argv[i]);
            }
            else
            {
                rc = mark_disk(fd);
                if (rc)
                {
                    fprintf(stderr,
                            "osmtool: Unable to mark \"%s\": %s\n",
                            argv[i], strerror(-rc));
                }
                else
                {
                    fprintf(stdout,
                            "osmtool: Marked \"%s\" successfully\n",
                            argv[i]);
                }
            }
        }
        else if (rc == -EEXIST)
        {
            if (op != OSMTOOL_UNMARK)
            {
                fprintf(stdout,
                        "osmtool: Disk \"%s\" is an OSM disk\n",
                        argv[i]);
            }
            else
            {
                rc = unmark_disk(fd);
                if (rc)
                {
                    fprintf(stderr,
                            "osmtool: Unable to unmark \"%s\": %s\n",
                            argv[i], strerror(-rc));
                }
                else
                {
                    fprintf(stdout,
                            "osmtool: Unarked \"%s\" successfully\n",
                            argv[i]);
                }
            }
        }
        else
        {
            fprintf(stderr, "osmtool: Unable to check \"%s\": %s\n",
                    argv[i], strerror(-rc));
        }

        close(fd);
    }

    return 0;
}  /* main() */


