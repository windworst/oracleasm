/* Copyright (c) 2003 Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmtool.c - Oracle Storage Manager user tool
    
  DESCRIPTION
  
  This is the main source file for the ASM user tool.
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
#include "oratypes.h"
#include "osmlib.h"
#include "linux/asmdisk.h"



/*
 * Enums
 */
typedef enum
{
    ASMTOOL_NOOP,
    ASMTOOL_CHECK,
    ASMTOOL_MARK,
    ASMTOOL_UNMARK
} ASMToolOperation;



/*
 * Functions
 */


/*
 * strnlen() is useful, and it's under __USE_GNU
 */
static inline size_t strnlen(const char *s, size_t size)
{
    char *p = memchr(s, '\0', size);
    return p ? p - s : size;
}  /* strnlen() */


static void print_usage(int rc)
{
    FILE *output = rc ? stderr : stdout;

    fprintf(output,
            "Usage: asmtool -M -l <disk1> -a <attribute> ...\n"
            "       asmtool -U -l <disk1>\n"
            "       asmtool -C -l <disk1> [-a <attribute>] ...\n"
            "       asmtool -h\n"
            "       asmtool -V\n");
    exit(rc);
}  /* print_usage() */


static void print_version()
{
    fprintf(stderr, "asmtool version %s\n", VERSION);
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
    {
        close(fd);
        return -errno;
    }
    
    if (!S_ISBLK(stat_buf.st_mode))
    {
        close(fd);
        return -ENOTBLK;
    }

    return fd;
}  /* open_disk() */


static int read_disk(int fd, struct asm_disk_label *adl)
{
    int rc, tot;

    if ((fd < 0) || !adl)
        return -EINVAL;

    rc = lseek(fd, ASM_DISK_LABEL_OFFSET, SEEK_SET);
    if (rc < 0)
        return -errno;

    tot = 0;
    while (tot < sizeof(struct asm_disk_label))
    {
        rc = read(fd, adl + tot, sizeof(struct asm_disk_label) - tot);
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

    if (tot < sizeof(struct asm_disk_label))
        return -ENOSPC;

    return 0;
}


static int write_disk(int fd, struct asm_disk_label *adl)
{
    int rc, tot;

    if ((fd < 0) || !adl)
        return -EINVAL;

    rc = lseek(fd, ASM_DISK_LABEL_OFFSET, SEEK_SET);
    if (rc < 0)
        return -errno;

    tot = 0;
    while (tot < sizeof(struct asm_disk_label))
    {
        rc = write(fd, adl + tot, sizeof(struct asm_disk_label) - tot);
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

    if (tot < sizeof(struct asm_disk_label))
        return -ENOSPC;

    return 0;
}


static int check_disk(struct asm_disk_label *adl, const char *label)
{
    int tot;

    if (!adl)
        return -EINVAL;

    /* Not an ASM disk */
    if (memcmp(adl->dl_tag, ASM_DISK_LABEL_MARKED, sizeof(adl->dl_tag)))
        return -ENXIO;

    if (label)
    {
        tot = strnlen(label, sizeof(adl->dl_id));
        if (tot != strnlen(adl->dl_id, sizeof(adl->dl_id)))
            return -ESRCH;
        if (memcmp(adl->dl_id, label, tot))
            return -ESRCH;
    }

    return 0;
}  /* check_disk() */


static int mark_disk(int fd, struct asm_disk_label *adl,
                     const char *label)
{
    int rc;
    size_t len;

    if ((fd < 0) || !adl || !label)
        return -EINVAL;

    rc = check_disk(adl, label);
    if (!rc)
        return -EEXIST;
    if ((rc == -ENXIO) || (rc == -ESRCH))
    {
        memcpy(adl->dl_tag, ASM_DISK_LABEL_MARKED, sizeof(adl->dl_tag));

        memset(adl->dl_id, 0, sizeof(adl->dl_id));
        len = strnlen(label, sizeof(adl->dl_id));
        memcpy(adl->dl_id, label, len);

        rc = write_disk(fd, adl);
    }

    return rc;
}  /* mark_disk() */


static int unmark_disk(int fd, struct asm_disk_label *adl)
{
    int rc;

    if ((fd < 0) || !adl)
        return -EINVAL;

    rc = check_disk(adl, NULL);
    if (rc == -ENXIO)
        return -ESRCH;
    if (!rc)
    {
        memcpy(adl->dl_tag, ASM_DISK_LABEL_CLEAR, sizeof(adl->dl_tag));
        memset(adl->dl_id, 0, sizeof(adl->dl_id));

        rc = write_disk(fd, adl);
    }

    return rc;
}  /* unmark_disk() */



extern char *optarg;
extern int optopt;
extern int opterr;
int parse_options(int argc, char *argv[], ASMToolOperation *op,
                  char **disk, char **label)
{
    int c;
    char *attr, *val;

    opterr = 0;
    while((c = getopt(argc, argv, ":hVMUCl:a:")) != EOF)
    {
        switch(c)
        {
            case 'h':
                print_usage(0);
                break;

            case 'V':
                print_version();
                break;

            case 'M':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_MARK;
                break;

            case 'U':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_UNMARK;
                break;

            case 'C':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_CHECK;
                break;

            case 'l':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *disk = optarg;
                break;

            case 'a':
                if (!optarg || !*optarg)
                    return -EINVAL;
                attr = optarg;
                val = strchr(attr, '=');
                if (val)
                {
                    *val = '\0';
                    val += 1;
                }
                if (strcmp(attr, "label"))
                {
                    fprintf(stderr,
                            "asmtool: Unknown attribute: \"%s\"\n",
                            attr);
                    return -EINVAL;
                }
                *label = val ? val : "";
                break;

            case '?':
                fprintf(stderr, "asmtool: Invalid option: \'-%c\'\n",
                        optopt);
                print_usage(1);
                break;

            case ':':
                fprintf(stderr,
                        "asmtool: Option \'-%c\' requires an argument\n",
                        optopt);
                print_usage(1);
                break;

            default:
                return -EINVAL;
                break;
        }
    }

    if (*op == ASMTOOL_NOOP)
    {
        fprintf(stderr, "You must specify an operation.\n");
        return -EINVAL;
    }
    if (!disk || !*disk)
    {
        fprintf(stderr, "You must specify a disk.\n");
        return -EINVAL;
    }
    if ((*op == ASMTOOL_MARK) && (!label || !*label))
    {
        fprintf(stderr, "You must specify a disk ID.\n");
        return -EINVAL;
    }

    return 0;
}  /* parse_options() */


/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int fd, rc;
    char *disk = NULL, *label = NULL;
    struct asm_disk_label adl;
    ASMToolOperation op = ASMTOOL_NOOP;

    rc = parse_options(argc, argv, &op, &disk, &label);
    if (rc)
        print_usage(rc);

    fd = open_disk(disk);
    if (fd < 0)
    {
        fprintf(stderr, "asmtool: Unable to open \"%s\": %s\n",
                disk, strerror(-fd));
    }

    rc = read_disk(fd, &adl);
    if (rc)
    {
        fprintf(stdout,
                "asmtool: Unable to read disk \"%s\": %s\n",
                disk, strerror(-rc));
        goto out_close;
    }

    switch (op)
    {
        case ASMTOOL_MARK:
            rc = mark_disk(fd, &adl, label);
            if (rc == -EEXIST)
            {
                fprintf(stdout,
                        "asmtool: Disk \"%s\" is marked an ASM disk\n",
                        disk);
            }
            else if (rc)
            {
                fprintf(stderr,
                        "asmtool: Unable to mark \"%s\": %s\n",
                        disk, strerror(-rc));
            }
            else
            {
                fprintf(stdout,
                        "asmtool: Marked \"%s\" successfully\n",
                        disk);
            }
            break;

        case ASMTOOL_UNMARK:
            rc = unmark_disk(fd, &adl);
            if (rc == -ESRCH)
            {
                fprintf(stdout,
                        "asmtool: Disk \"%s\" is not marked an ASM disk\n",
                        disk);
            }
            else if (rc)
            {
                fprintf(stderr,
                        "asmtool: Unable to unmark \"%s\": %s\n",
                        disk, strerror(-rc));
            }
            else
            {
                fprintf(stdout,
                        "asmtool: Unarked \"%s\" successfully\n",
                        disk);
            }
            break;

        case ASMTOOL_CHECK:
            rc = check_disk(&adl, label);
            if (!rc)
            {
                fprintf(stdout,
                        "asmtool: Disk \"%s\" is marked an ASM disk\n",
                        disk);
            }
            else if (rc == -ENXIO)
            {
                fprintf(stdout,
                        "asmtool: Disk \"%s\" is not marked an ASM disk\n",
                        disk);
            }
            else if (rc == -ESRCH)
            {
                if (*label)
                {
                    fprintf(stdout,
                            "asmtool: Disk \"%s\" is marked an ASM disk, but does not match the label \"%s\"\n",
                            disk, label);
                }
                else
                {
                    char *id = (char *)malloc(sizeof(char) *
                                              (sizeof(adl.dl_id) + 1));
                    if (!id)
                    {
                        fprintf(stderr,
                                "asmtool: Unable to allocate memory\n");
                        goto out_close;
                    }

                    memcpy(id, adl.dl_id, sizeof(adl.dl_id));
                    id[sizeof(adl.dl_id)] = '\0';

                    fprintf(stdout,
                            "asmtool: Disk \"%s\" is marked an ASM disk with the label \"%s\"\n",
                            disk, id);
                    free(id);
                }
            }
            else
            {
                fprintf(stderr, "asmtool: Unable to check disk \"%s\": %s\n",
                        disk, strerror(-rc));
            }
            break;

        default:
            fprintf(stderr, "Invalid operation\n");
            break;
    }

out_close:
    close(fd);

    return 0;
}  /* main() */


