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
#include <sys/statfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <linux/types.h>
#include "oratypes.h"
#include "asmlib.h"
#include "linux/asmdisk.h"
#include "linux/asmmanager.h"

#include "list.h"


/*
 * Defines
 */
#define ASMFS_MAGIC     0x958459f6



/*
 * Enums
 */
typedef enum
{
    ASMTOOL_NOOP,
    ASMTOOL_INFO,
    ASMTOOL_LIST_MANAGERS,
    ASMTOOL_CREATE,
    ASMTOOL_DELETE,
} ASMToolOperation;



/*
 * Typedefs
 */
typedef struct _OptionAttr      OptionAttr;
typedef struct _ASMToolAttrs    ASMToolAttrs;
typedef struct _ASMDisk         ASMDisk;
typedef struct _ASMDevice       ASMDevice;



/*
 * Structures
 */
struct _OptionAttr
{
    struct list_head oa_list;
    int oa_set;
    char *oa_name;
    char *oa_value;
};

struct _ASMToolAttrs
{
    /* Booleans */
    int force;
    int mark;
    /* All textual attrs */
    struct list_head attr_list;
};

struct _ASMDisk
{
    char *di_name;
    dev_t di_dev;
    ASMDevice *di_device;
};

struct _ASMDevice
{
    char *de_name;
    char *de_label;
    dev_t de_dev;
    ASMDisk *de_disk;
};



/*
 * Function prototypes
 */
static inline size_t strnlen(const char *s, size_t size);
static void print_usage(int rc);
static void print_version();
static int open_disk(const char *disk_name);
static int read_disk(int fd, struct asm_disk_label *adl);
static int write_disk(int fd, struct asm_disk_label *adl);
static int check_disk(struct asm_disk_label *adl, const char *label);
static int mark_disk(int fd, struct asm_disk_label *adl,
                     const char *label);
static int unmark_disk(int fd, struct asm_disk_label *adl);
static char *asm_disk_id(struct asm_disk_label *adl);
static int delete_disk_by_name(const char *manager,
                               const char *disk,
                               ASMToolAttrs *attrs);
static int delete_disk_by_device(const char *manager,
                                 const char *target,
                                 ASMToolAttrs *attrs);
static int make_disk(const char *manager, const char *disk,
                     const char *target);
static int unlink_disk(const char *manager, const char *disk);
static int create_disk(const char *manager, const char *disk,
                       const char *target, ASMToolAttrs *attrs);
static int delete_disk(const char *manager, const char *object,
                       ASMToolAttrs *attrs);
static int get_info_asmdisk_by_device(const char *manager,
                                      const char *target,
                                      ASMToolAttrs *attrs);
static int get_info_asmdisk_by_name(const char *manager,
                                    const char *disk,
                                    ASMToolAttrs *attrs);
static int get_info_asmdisk(const char *manager, const char *object,
                            ASMToolAttrs *attrs);
static int get_info(const char *manager, const char *object,
                    const char *stype, ASMToolAttrs *attrs);
static int is_manager(const char *filename);
static int list_managers();
static int parse_options(int argc, char *argv[], ASMToolOperation *op,
                         char **manager, char **object, char **target,
                         char **stype, struct list_head *prog_attrs);
static int prepare_attrs(char **attr_names, int num_names,
                         struct list_head *l);
static void clear_attrs(struct list_head *l);
static int attr_set(struct list_head *attr_list, const char *attr_name);
static const char *attr_string(struct list_head *attr_list,
                               const char *attr_name,
                               const char *def_value);
static int attr_boolean(struct list_head *attr_list,
                        const char *attr_name,
                        int def_value);
static int fill_attrs(ASMToolAttrs *attrs);



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
            "Usage: asmtool -M\n"
            "       asmtool -C -l <manager> -n <object> -s <device> [-a <attribute> ...]\n"
            "       asmtool -D -l <manager> -n <object>\n"
            "       asmtool -I -l <manager> [-n <object>] [-t <type>] [-a <attribute>] ...\n"
            "       asmtool -h\n"
            "       asmtool -V\n");
    exit(rc);
}  /* print_usage() */


static void print_version()
{
    fprintf(stderr, "asmtool version %s\n", VERSION);
    exit(0);
}  /* print_version() */


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
}  /* read_disk() */


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
}  /* write_disk() */


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

    if ((fd < 0) || !adl)
        return -EINVAL;

    if (!label || !*label)
    {
        fprintf(stderr, "You must specify a disk ID.\n");
        return -EINVAL;
    }

    memcpy(adl->dl_tag, ASM_DISK_LABEL_MARKED, sizeof(adl->dl_tag));

    memset(adl->dl_id, 0, sizeof(adl->dl_id));
    len = strnlen(label, sizeof(adl->dl_id));
    memcpy(adl->dl_id, label, len);

    rc = write_disk(fd, adl);

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


static char *asm_disk_id(struct asm_disk_label *adl)
{
    char *id;

    id = (char *)malloc(sizeof(char) *
                        (sizeof(adl->dl_id) + 1));
    if (id)
    {
        memcpy(id, adl->dl_id, sizeof(adl->dl_id));
        id[sizeof(adl->dl_id)] = '\0';
    }

    return id;
}  /* asm_disk_id() */


static int delete_disk_by_name(const char *manager, const char *disk,
                               ASMToolAttrs *attrs)
{
    int rc, fd = -1;
    char *asm_disk;
    struct asm_disk_label adl;

    rc = -ENOMEM;
    asm_disk = asm_disk_path(manager, disk);
    if (!asm_disk)
    {
        fprintf(stderr,
                "asmtool: Unable to determine ASM disk: %s\n",
                strerror(-rc));
        goto out;
    }

    rc = open_disk(asm_disk);
    if (rc < 0)
    {
        switch (rc)
        {
            /* Things that mean we know it's bad */
            case -ENXIO:
            case -ENODEV:
                goto out_del;

            default:
                fprintf(stderr, "Unable to open ASM disk \"%s\": %s\n",
                        disk, strerror(-rc));
                goto out;
        }
    }
    fd = rc;

    rc = read_disk(fd, &adl);
    if (rc)
    {
        switch (rc)
        {
            case -EINVAL:
            case -EBADF:
                goto out_del;

            default:
                fprintf(stderr,
                        "asmtool: Unable to query ASM disk \"%s\": %s\n",
                        disk, strerror(-rc));
                goto out_close;
        }
    }

    rc = check_disk(&adl, disk);
    if (!rc)
    {
        rc = unmark_disk(fd, &adl);
        if (rc && (rc != -ESRCH))
        {
            fprintf(stderr,
                    "asmtool: Unable to unmark ASM disk \"%s\": %s\n",
                    disk, strerror(-rc));
            goto out_close;
        }
    }

out_del:
    rc = unlink_disk(manager, disk);

out_close:
    if (fd > -1)
        close(fd);

out:
    return rc;
}  /* delete_disk_by_name() */


static int delete_disk_by_device(const char *manager,
                                 const char *target,
                                 ASMToolAttrs *attrs)
{
    int rc, dev_fd, disk_fd;
    char *id, *asm_disk;
    struct asm_disk_label adl;
    struct stat dev_stat_buf, disk_stat_buf;

    rc = open_disk(target);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmtool: Unable to open device \"%s\": %s\n",
                target, strerror(-rc));
        goto out;
    }
    dev_fd = rc;

    rc = read_disk(dev_fd, &adl);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Unable to query device \"%s\": %s\n",
                target, strerror(-rc));
        goto out_close;
    }

    rc = check_disk(&adl, NULL);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Device \"%s\" is not labeled for ASM\n",
                target);
        goto out_close;
    }

    rc = -ENOMEM;
    id = asm_disk_id(&adl);
    if (!id)
    {
        fprintf(stderr,
                "asmtool: Unable to determine ASM disk: %s\n",
                strerror(ENOMEM));
        goto out_close;
    }

    rc = -ENODEV;
    if (!*id)
    {
        fprintf(stderr,
                "asmtool: Invalid ASM disk: missing ID\n");
        goto out_free;
    }

    rc = -ENOMEM;
    asm_disk = asm_disk_path(manager, id);
    if (!asm_disk)
    {
        fprintf(stderr,
                "asmtool: Unable to determine ASM disk: %s\n",
                strerror(-rc));
        goto out_free;
    }

    rc = open_disk(asm_disk);
    if (rc < 0)
    {
        if (rc == -ENXIO)
        {
            /* If the actual device behind asm_disk is not there... */
            rc = unlink_disk(manager, id);
            if (rc)
                goto out_free;
        }
        else if (rc != -ENOENT)
        {
            fprintf(stderr,
                    "asmtool: Unable to open ASM disk \"%s\": %s\n",
                    id, strerror(-rc));
            goto out_free;
        }
    }
    else
    {
        /* We've successfully opened the ASM disk */
        disk_fd = rc;

        rc = fstat(disk_fd, &disk_stat_buf);
        close(disk_fd);
        if (rc)
        {
            fprintf(stderr,
                    "asmtool: Unable to stat ASM disk \"%s\": %s\n",
                    id, strerror(errno));
            goto out_free;
        }
        rc = fstat(dev_fd, &dev_stat_buf);
        if (rc)
        {
            fprintf(stderr,
                    "asmtool: Unable to stat device \"%s\": %s\n",
                    target, strerror(errno));
            goto out_free;
        }

        if (dev_stat_buf.st_rdev != disk_stat_buf.st_rdev)
        {
            /* Force means clear the device, but ignore the ASM disk */
            if (!attrs->force)
            {
                rc = -ENODEV;
                fprintf(stderr,
                        "asmtool: ASM disk \"%s\" does not match device \"%s\"\n",
                        id, target);
                goto out_free;
            }
        }
        else
        {
            /*
             * Ok, if /dev/foo was labeled 'vol1', and opening
             * <manager>/disks/vol1 yields the same st_rdev as /dev/foo,
             * Then <manager>/disks/vol1 is, indeed, the associated
             * ASM disk.  Now let's blow it away.
             */
            rc = unlink_disk(manager, id);
            if (rc)
                goto out_free;
        }
    }

    rc = unmark_disk(dev_fd, &adl);
    if (rc)
    {
        fprintf(stderr, "Unable to clear device \"%s\": %s\n",
                target, strerror(-rc));
    }

out_free:
    free(id);

out_close:
    close(dev_fd);

out:
    return rc;
}  /* delete_disk_by_device() */


static int make_disk(const char *manager, const char *disk,
                     const char *target)
{
    int rc, t_fd;
    char *asm_disk;
    struct stat stat_buf;

    asm_disk = asm_disk_path(manager, disk);
    if (!asm_disk)
        return -ENOMEM;

    rc = open_disk(target);
    if (rc < 0)
    {
        fprintf(stderr, "asmtool: Unable to open device \"%s\": %s\n",
                target, strerror(-rc));
        goto out;
    }
    t_fd = rc;

    /* FIXME: Check if mounted ? */

    rc = fstat(t_fd, &stat_buf);
    if (rc)
    {
        fprintf(stderr, "asmtool: Unable to query device \"%s\": %s\n",
                target, strerror(errno));
        goto out_close;
    }

    rc = mknod(asm_disk, 0660 | S_IFBLK, stat_buf.st_rdev);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Unable to create ASM disk \"%s\": %s\n",
                disk, strerror(errno));
        goto out_close;
    }

    rc = open_disk(asm_disk);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmtool: Unable to open ASM disk \"%s\": %s\n",
                disk, strerror(-rc));
    }

out_close:
    close(t_fd);

out:
    free(asm_disk);
    return rc;
}  /* make_disk() */


static int unlink_disk(const char *manager, const char *disk)
{
    int rc;
    char *asm_disk;

    asm_disk = asm_disk_path(manager, disk);
    if (!asm_disk)
        return -ENOMEM;

    rc = unlink(asm_disk);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Unable to remove ASM disk \"%s\": %s\n",
                disk, strerror(errno));
    }

    free(asm_disk);
    return rc;
}  /* unlink_disk() */


static int create_disk(const char *manager, const char *disk,
                       const char *target, ASMToolAttrs *attrs)
{
    int rc, fd;
    char *id;
    struct asm_disk_label adl;

    if (!disk || !*disk)
    {
        fprintf(stderr,
                "asmtool: You must specify an ASM disk name (-n).\n");
        return -EINVAL;
    }
    if (strchr(disk, '/'))
    {
        fprintf(stderr, "asmtool: Invalid ASM disk name: \"%s\"\n",
                disk);
        return -EINVAL;
    }
    if (!target || !*target)
    {
        fprintf(stderr,
                "asmtool: You must specify a target device (-s).\n");
        return -EINVAL;
    }

    rc = make_disk(manager, disk, target);
    if (rc < 0)
        goto out;
    fd = rc;

    rc = read_disk(fd, &adl);
    if (rc)
    {
        fprintf(stdout,
                "asmtool: Unable to read device \"%s\": %s\n",
                target, strerror(-rc));
        goto out_close;
    }

    rc = check_disk(&adl, disk);
    if (rc)
    {
        if (rc == -ESRCH)
        {
            rc = -ENOMEM;
            id = asm_disk_id(&adl);
            if (!id)
            {
                fprintf(stderr,
                        "asmtool: Unable to query ASM disk: %s\n",
                        strerror(-rc));
            }
            else
            {
                fprintf(stderr,
                        "asmtool: Device \"%s\" is already labeled for ASM disk \"%s\"\n",
                        target, id);
                free(id);
            }
            goto out_close;
        }
        else if (rc != -ENXIO)
        {
            fprintf(stderr,
                    "asmtool: Unable to validate device \"%s\": %s\n",
                    target, strerror(-rc));
            goto out_close;
        }
        if (!attrs->mark)
        {
            fprintf(stderr,
                    "asmtool: Device \"%s\" is not labeled as an ASM disk\n",
                    target);
            goto out_close;
        }
        rc = mark_disk(fd, &adl, disk);
        if (rc)
        {
            if (rc != -EEXIST)
            {
                fprintf(stderr,
                        "asmtool: Unable to label device \"%s\": %s\n",
                        target, strerror(-rc));
            }
            else
                rc = 0;
        }
    }

out_close:
    close(fd);

    if (rc)
        unlink_disk(manager, disk);

out:
    return rc;
}  /* create_disk() */


static int delete_disk(const char *manager, const char *object,
                       ASMToolAttrs *attrs)
{
    int rc;

    if (!object || !*object)
    {
        fprintf(stderr,
                "asmtool: You must specify an object to delete (-n).\n");
        return -EINVAL;
    }

    if (strchr(object, '/'))
        rc = delete_disk_by_device(manager, object, attrs);
    else
        rc = delete_disk_by_name(manager, object, attrs);

    return rc;
}  /* delete_disk() */


static int get_info_asmdisk_by_name(const char *manager,
                                    const char *disk,
                                    ASMToolAttrs *attrs)
{
    int rc, fd;
    char *label, *asm_disk;
    struct asm_disk_label adl;

    rc = -EINVAL;
    label = (char *)attr_string(&attrs->attr_list, "label", NULL);
    if (label && strcmp(disk, label))
    {
        fprintf(stderr,
                "asmtool: Looking for disk \"%s\" with label \"%s\" makes no sense\n",
                disk, label);
        goto out;
    }

    rc = -ENOMEM;
    asm_disk = asm_disk_path(manager, disk);
    if (!asm_disk)
    {
        fprintf(stderr, "asmtool: Unable to query ASM disk \"%s\": %s\n",
                disk, strerror(-rc));
        goto out;
    }

    rc = open_disk(asm_disk);
    if (rc < 0)
    {
        fprintf(stderr, "asmtool: Unable to open ASM disk \"%s\": %s\n",
                disk, strerror(-rc));
        goto out;
    }
    fd = rc;

    rc = read_disk(fd, &adl);
    if (rc)
    {
        fprintf(stderr, "asmtool: Unable to read ASM disk \"%s\": %s\n",
                disk, strerror(-rc));
        goto out_close;
    }

    rc = check_disk(&adl, disk);
    if (rc)
    {
        if (rc == -ENXIO)
        {
            fprintf(stderr,
                    "asmtool: ASM disk \"%s\" defines an unmarked device\n",
                    disk);
        }
        else if (rc == -ESRCH)
        {
            label = asm_disk_id(&adl);
            fprintf(stdout,
                    "asmtool: ASM disk \"%s\" is labeled for ASM disk \"%s\"\n",
                    disk, label ? label : "<unknown>");
            if (label)
                free(label);
        }
        else
        {
            fprintf(stderr,
                    "asmtool: Error querying ASM disk \"%s\": %s\n",
                    disk, strerror(-rc));
        }
    }
    else
    {
        fprintf(stdout,
                "asmtool: Disk \"%s\" is a valid ASM disk\n",
                disk);
    }

out_close:
    close (fd);

out:
    return rc;
}  /* get_info_asmdisk_by_name() */


static int get_info_asmdisk_by_device(const char *manager,
                                      const char *target,
                                      ASMToolAttrs *attrs)
{
    int rc, fd;
    char *id;
    struct asm_disk_label adl;

    rc = open_disk(target);
    if (rc < 0)
    {
        fprintf(stderr, "asmtool: Unable to open device \"%s\": %s\n",
                target, strerror(-rc));
        goto out;
    }
    fd = rc;

    rc = read_disk(fd, &adl);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Unable to read device \"%s\": %s\n",
                target, strerror(-rc));
        goto out_close;
    }

    rc = check_disk(&adl, NULL);
    if (!rc)
    {
        if (attr_set(&attrs->attr_list, "label"))
        {
            if (attr_string(&attrs->attr_list, "label", NULL))
            {
                rc = -EINVAL;
                fprintf(stderr,
                        "asmtool: Cannot set attribute \"label\"\n");
                goto out_close;
            }
            id = asm_disk_id(&adl);
            if (!id)
            {
                fprintf(stderr,
                        "asmtool: Unable to allocate memory\n");
                goto out_close;
            }

            fprintf(stdout,
                    "asmtool: Disk \"%s\" is marked an ASM disk with the label \"%s\"\n",
                    target, id);
            free(id);
        }
        else
        {
            fprintf(stdout,
                    "asmtool: Disk \"%s\" is marked an ASM disk\n",
                    target);
        }
    }
    else if (rc == -ENXIO)
    {
        fprintf(stderr,
                "asmtool: Disk \"%s\" is not marked an ASM disk\n",
                target);
    }
    else
    {
        fprintf(stderr, "asmtool: Unable to check disk \"%s\": %s\n",
                target, strerror(-rc));
    }

out_close:
    close(fd);

out:
    return rc;
}  /* get_info_asmdisk_by_device() */


static int get_info_asmdisk(const char *manager, const char *object,
                            ASMToolAttrs *attrs)
{
    if (strchr(object, '/'))
        return get_info_asmdisk_by_device(manager, object, attrs);
    else
        return get_info_asmdisk_by_name(manager, object, attrs);
}  /* get_info_asmdisk() */


static int get_info(const char *manager, const char *object,
                    const char *stype, ASMToolAttrs *attrs)
{
    int rc;

    rc = -ENOSYS;
    if (!object || !*object)
    {
        fprintf(stderr, "asmtool: No object specified: %s\n",
                strerror(-rc));
        goto out;
    }

    /* So far, we only support one query */
    if (stype && (!strcmp(stype, "asmdisk")))
    {
        fprintf(stderr,
                "asmtool: Unsupported or invalid object type: %s\n",
                stype);
        goto out;
    }

    rc = get_info_asmdisk(manager, object, attrs);

out:
    return rc;
}  /* get_info() */


static int is_manager(const char *filename)
{
    int rc;
    struct statfs statfs_buf;

    if (!filename || !*filename)
        return -EINVAL;

    /* I wanted to use statvfs, but there is no f_type */
    rc = statfs(filename, &statfs_buf);
    if (rc)
        return -errno;

    if (statfs_buf.f_type != ASMFS_MAGIC)
        return -ENXIO;

    return 0;
}  /* is_manager() */


static int list_managers()
{
    int rc;
    char *t_manager;

    /* /dev/oracleasm only for now */
    t_manager = "/dev/oracleasm";

    rc = is_manager(t_manager);
    if (rc)
        return rc;

    fprintf(stdout, "%s\n", t_manager);

    return 0;
}  /* list_managers() */


extern char *optarg;
extern int optopt;
extern int opterr;
static int parse_options(int argc, char *argv[], ASMToolOperation *op,
                         char **manager, char **object, char **target,
                         char **stype, struct list_head *prog_attrs)
{
    int c;
    char *attr_name, *attr_value;
    struct list_head *pos;
    OptionAttr *attr;

    opterr = 0;
    while((c = getopt(argc, argv, ":hVMCDIl:n:s:t:a:")) != EOF)
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
                *op = ASMTOOL_LIST_MANAGERS;
                break;

            case 'C':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_CREATE;
                break;

            case 'D':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_DELETE;
                break;

            case 'I':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_INFO;
                break;

            case 'l':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *manager = optarg;
                break;

            case 'n':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *object = optarg;
                break;

            case 's':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *target = optarg;
                break;

            case 't':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *stype = optarg;
                break;

            case 'a':
                if (!optarg || !*optarg)
                    return -EINVAL;
                attr_name = optarg;
                attr_value = strchr(attr_name, '=');
                if (attr_value)
                {
                    *attr_value = '\0';
                    attr_value += 1;
                }

                attr = NULL;
                list_for_each(pos, prog_attrs) {
                    attr = list_entry(pos, OptionAttr, oa_list);
                    if (!strcmp(attr->oa_name, attr_name))
                        break;
                    attr = NULL;
                }
                if (!attr)
                {
                    fprintf(stderr,
                            "asmtool: Unknown attribute: \"%s\"\n",
                            attr_name);
                    return -EINVAL;
                }
                attr->oa_set = TRUE;
                if (attr_value)
                {
                    attr->oa_value = strdup(attr_value);
                    if (!attr->oa_value)
                        return -ENOMEM;
                }
                break;

            case '?':
                fprintf(stderr, "asmtool: Invalid option: \'-%c\'\n",
                        optopt);
                return -EINVAL;
                break;

            case ':':
                fprintf(stderr,
                        "asmtool: Option \'-%c\' requires an argument\n",
                        optopt);
                return -EINVAL;
                break;

            default:
                return -EINVAL;
                break;
        }
    }

    return 0;
}  /* parse_options() */


static int prepare_attrs(char **attr_names, int num_names,
                         struct list_head *l)
{
    int i;
    OptionAttr *attr;

    for (i = 0; i < num_names; i++)
    {
        attr = (OptionAttr *)malloc(sizeof(OptionAttr));
        if (!attr)
            return -ENOMEM;
        memset(attr, 0, sizeof(*attr));
        attr->oa_name = strdup(attr_names[i]);
        if (!attr->oa_name)
            return -ENOMEM;
        list_add(&attr->oa_list, l);
    }

    return 0;
}  /* prepare_attrs() */


static void clear_attrs(struct list_head *l)
{
    struct list_head *pos, *tmp;
    OptionAttr *attr;

    list_for_each_safe(pos, tmp, l) {
        attr = list_entry(pos, OptionAttr, oa_list);
        if (attr->oa_name)
            free(attr->oa_name);
        if (attr->oa_value)
            free(attr->oa_value);
        list_del(&attr->oa_list);
        free(attr);
    }
}  /* clear_attrs() */


static int attr_set(struct list_head *attr_list, const char *attr_name)
{
    OptionAttr *attr;
    struct list_head *pos;

    list_for_each(pos, attr_list) {
        attr = list_entry(pos, OptionAttr, oa_list);
        if (!strcmp(attr->oa_name, attr_name))
        {
            return attr->oa_set;
        }
    }

    return 0;
}  /* attr_set() */


static const char *attr_string(struct list_head *attr_list,
                               const char *attr_name,
                               const char *def_value)
{
    OptionAttr *attr;
    struct list_head *pos;

    list_for_each(pos, attr_list) {
        attr = list_entry(pos, OptionAttr, oa_list);
        if (!strcmp(attr->oa_name, attr_name))
        {
            if (!attr->oa_set)
                return def_value;
            return attr->oa_value;
        }
    }

    return def_value;
}  /* attr_string() */
  

static int attr_boolean(struct list_head *attr_list,
                        const char *attr_name,
                        int def_value)
{
    int i;
    struct list_head *pos;
    OptionAttr *attr;
    struct b_table
    {
        char *match;
        int value;
    } bt[] = {
        {"0", 0}, {"1", 1},
        {"f", 0}, {"t", 1},
        {"false", 0}, {"true", 1},
        {"n", 0}, {"y", 1},
        {"no", 0}, {"yes", 1},
        {"off", 0}, {"on", 1}
    };

    list_for_each(pos, attr_list) {
        attr = list_entry(pos, OptionAttr, oa_list);
        if (!strcmp(attr->oa_name, attr_name))
        {
            if (!attr->oa_set || !attr->oa_value || !*(attr->oa_value))
                return 1;
            for (i = 0; i < (sizeof(bt) / sizeof(*bt)); i++)
            {
                if (!strcmp(bt[i].match, attr->oa_value))
                    return bt[i].value;
            }
            fprintf(stderr,
                    "asmtool: Invalid value for attribute \"%s\": %s\n",
                    attr_name, attr->oa_value);
            return -EINVAL;
        }
    }

    return def_value;
}  /* attr_boolean() */


static int fill_attrs(ASMToolAttrs *attrs)
{
    attrs->force = attr_boolean(&attrs->attr_list, "force", 0);
    if (attrs->force < 0)
        return attrs->force;
    attrs->mark = attr_boolean(&attrs->attr_list, "mark", 1);
    if (attrs->mark < 0)
        return attrs->mark;
    return 0;
}  /* fill_attrs() */


/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int rc;
    char *manager = NULL, *object = NULL, *target = NULL, *stype = NULL;
    ASMToolOperation op = ASMTOOL_NOOP;
    char *attr_names[] = { "label", "mark", "force" };
    ASMToolAttrs attrs = {
        .force = FALSE,
        .mark = TRUE
    };

    INIT_LIST_HEAD(&attrs.attr_list);

    rc = prepare_attrs(attr_names,
                       sizeof(attr_names)/sizeof(*attr_names),
                       &attrs.attr_list);
    if (rc)
    {
        fprintf(stderr, "asmtool: Unable to initialize: %s\n",
                strerror(-rc));
        goto out;
    }

    rc = parse_options(argc, argv, &op, &manager, &object,
                       &target, &stype, &attrs.attr_list);
    if (rc)
        print_usage(rc);

    if (op == ASMTOOL_LIST_MANAGERS)
    {
        if (manager || object || target)
        {
            fprintf(stderr, "asmtool: Too many arguments\n");
            print_usage(-EINVAL);
        }
        rc = list_managers();
        if (rc)
            fprintf(stderr, "asmtool: Error listing managers: %s\n",
                    strerror(-rc));
        goto out;
    }

    rc = fill_attrs(&attrs);
    if (rc)
        goto out;

    if (op == ASMTOOL_LIST_MANAGERS)
    {
        rc = list_managers();
        if (rc)
            fprintf(stderr, "asmtool: Error listing managers: %s\n",
                    strerror(-rc));
        goto out;
    }

    if (!manager || !*manager)
    {
        fprintf(stderr, "You must specify a manager.\n");
        return -EINVAL;
    }
    rc = is_manager(manager);
    if (rc)
    {
        fprintf(stderr, "Error opening manager: %s\n",
                strerror(-rc));
        goto out;
    }

    switch (op)
    {
        case ASMTOOL_NOOP:
            fprintf(stderr, "You must specify an operation.\n");
            print_usage(1);
            break;
            
        case ASMTOOL_CREATE:
            rc = create_disk(manager, object, target, &attrs);
            break;

        case ASMTOOL_DELETE:
            rc = delete_disk(manager, object, &attrs);
            break;

        case ASMTOOL_INFO:
            rc = get_info(manager, object, stype, &attrs);
            break;

        case ASMTOOL_LIST_MANAGERS:
            fprintf(stderr, "asmtool: Can't get here (ASMTOOL_LIST_MANAGERS)!\n");
            break;

        default:
            fprintf(stderr, "Invalid operation\n");
            break;
    }

out:
    clear_attrs(&attrs.attr_list);

    return rc;
}  /* main() */


