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


/*
 * Defines
 */
#define OSM_DISK_LABEL          "ORCLDISK"
#define OSM_DISK_LABEL_SIZE     8
#define OSM_DISK_LABEL_OFFSET   32

/*
 * Functions
 */


static void print_usage()
{
    fprintf(stderr, "Usage: osmtool <disk1> ...\n");
    exit(1);
}  /* print_usage() */


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



/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int i, fd, rc;

    if (argc < 2)
        print_usage();

    for (i = 1; i < argc; i++)
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
            rc = mark_disk(fd);
            if (rc)
            {
                fprintf(stderr, "osmtool: Unable to mark \"%s\": %s\n",
                        argv[i], strerror(-rc));
            }
            else
            {
                fprintf(stdout,
                        "osmtool: Marked \"%s\" successfully\n",
                        argv[i]);
            }
        }
        else
        {
            if (rc == -EEXIST)
            {
                fprintf(stdout,
                        "osmtool: Disk \"%s\" is already an OSM disk\n",
                        argv[i]);
            }
            else
            {
                fprintf(stderr, "osmtool: Unable to check \"%s\": %s\n",
                        argv[i], strerror(-rc));
            }
        }

        close(fd);
    }

    return 0;
}  /* main() */


