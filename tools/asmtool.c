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
#include "osmlib.h"
#include "linux/asmdisk.h"

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
    ASMTOOL_UNMARK
} ASMToolOperation;



/*
 * Typedefs
 */
typedef struct _OptionAttr OptionAttr;
typedef struct _ASMToolAttrs ASMToolAttrs;



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
    char *label;
    int force;
    int mark;
};




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
            "       asmtool -C -l <manager> -n <disk> -a <attribute> ...\n"
            "       asmtool -U -l <manager> -n <disk>\n"
            "       asmtool -I -l <manager> -n <disk> [-a <attribute>] ...\n"
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


static char *asm_disk_path(const char *manager, const char *disk)
{
    int len;
    char *asm_disk;
    char *sub_path = "/disks/";

    len = strlen(manager) + strlen(sub_path) + strlen(disk);
    asm_disk = (char *)malloc(sizeof(char) * (len + 1));
    if (!asm_disk)
        return NULL;
    snprintf(asm_disk, len + 1, "%s%s%s", manager, sub_path, disk);

    return asm_disk;
} /* asm_disk_path() */


static int make_disk(const char *manager, const char *disk,
                     int target_fd)
{
    char *asm_disk;

    asm_disk = asm_disk_path(manager, disk);
    if (!asm_disk)
        return -ENOMEM;

    free(asm_disk);
    return 0;
}  /* make_disk() */


static int create_disk(const char *manager, const char *disk,
                       const char *target, ASMToolAttrs *attrs)
{
    int rc, fd;
    char *id;
    struct asm_disk_label adl;

    if (!disk || !*disk)
    {
        fprintf(stderr, "You must specify an ASM disk name (-n).\n");
        return -EINVAL;
    }
    if (!target || !*target)
    {
        fprintf(stderr, "You must specify a target device (-s).\n");
        return -EINVAL;
    }

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
            id = (char *)malloc(sizeof(char) * (sizeof(adl.dl_id) + 1));
            if (!id)
                goto out_close;

            rc = -ESRCH;
            memcpy(id, adl.dl_id, sizeof(adl.dl_id));
            id[sizeof(adl.dl_id)] = '\0';

            fprintf(stderr,
                    "asmtool: Device \"%s\" is already labeled for ASM disk \"%s\"\n",
                    target, id);
            free(id);
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
        if (rc && (rc != -EEXIST))
        {
            fprintf(stderr,
                    "asmtool: Unable to label device \"%s\": %s\n",
                    target, strerror(-rc));
            goto out_close;
        }
    }

    rc = make_disk(manager, disk, fd);
    if (rc)
    {
        fprintf(stderr,
                "asmtool: Unable to create ASM disk \"%s\": %s\n",
                disk, strerror(-rc));
    }

out_close:
    close(fd);

out:
    return rc;
}  /* create_disk() */


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
                         char **manager, char **disk, char **target,
                         struct list_head *prog_attrs)
{
    int c;
    char *attr_name, *attr_value;
    struct list_head *pos;
    OptionAttr *attr;

    opterr = 0;
    while((c = getopt(argc, argv, ":hVMCUIl:n:s:a:")) != EOF)
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

            case 'U':
                if (*op != ASMTOOL_NOOP)
                    return -EINVAL;
                *op = ASMTOOL_UNMARK;
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
                *disk = optarg;
                break;

            case 's':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *target = optarg;
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
                attr->oa_value = attr_value;
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


static int attr_boolean(const char *attr_value)
{
    int i;
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

    if (!attr_value || !*attr_value)
        return -EINVAL;

    for (i = 0; i < (sizeof(bt) / sizeof(*bt)); i++)
    {
        if (!strcmp(bt[i].match, attr_value))
            return bt[i].value;
    }

    return -EINVAL;
}  /* attr_boolean() */


static int fill_attrs(ASMToolAttrs *attrs, struct list_head *attr_list)
{
    struct list_head *pos;
    OptionAttr *attr;

    list_for_each(pos, attr_list) {
        attr = list_entry(pos, OptionAttr, oa_list);
        if (!strcmp(attr->oa_name, "label"))
        {
            if (attr->oa_set)
                attrs->label = attr->oa_value ? attr->oa_value : "";
            break;
        }
        else if (!strcmp(attr->oa_name, "force"))
        {
            if (attr->oa_set)
            {
                attrs->force = attr_boolean(attr->oa_value);
                if (attrs->force < 0)
                    return attrs->force;
            }
        }
        else if (!strcmp(attr->oa_name, "mark"))
        {
            if (attr->oa_set)
            {
                attrs->mark = attr_boolean(attr->oa_value);
                if (attrs->mark < 0)
                    return attrs->mark;
            }
        }
    }

    return 0;
}  /* fill_attrs() */


/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int fd, rc;
    char *manager = NULL, *disk = NULL, *target = NULL;
    struct asm_disk_label adl;
    ASMToolOperation op = ASMTOOL_NOOP;
    struct list_head def_attrs;
    char *attr_names[] = { "label", "mark", "force" };
    ASMToolAttrs attrs = {
        .label = NULL,
        .force = FALSE,
        .mark = TRUE
    };

    INIT_LIST_HEAD(&def_attrs);

    rc = prepare_attrs(attr_names,
                       sizeof(attr_names)/sizeof(*attr_names),
                       &def_attrs);
    if (rc)
    {
        fprintf(stderr, "asmtool: Unable to initialize: %s\n",
                strerror(-rc));
        goto out;
    }

    rc = parse_options(argc, argv, &op, &manager, &disk,
                       &target, &def_attrs);
    if (rc)
        print_usage(rc);

    if (op == ASMTOOL_LIST_MANAGERS)
    {
        if (manager || disk || target)
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

    rc = fill_attrs(&attrs, &def_attrs);
    if (rc)
    {
        fprintf(stderr, "asmtool: Unable to process attributes: %s\n",
                strerror(-rc));
        goto out;
    }

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

    if (!disk || !*disk)
    {
        fprintf(stderr, "You must specify a disk (-n).\n");
        return -EINVAL;
    }
    fd = open_disk(disk);
    if (fd < 0)
    {
        fprintf(stderr, "asmtool: Unable to open \"%s\": %s\n",
                disk, strerror(-fd));
        goto out;
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
        case ASMTOOL_NOOP:
            fprintf(stderr, "You must specify an operation.\n");
            print_usage(1);
            break;
            
        case ASMTOOL_CREATE:
            rc = create_disk(manager, disk, target, &attrs);
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

        case ASMTOOL_INFO:
            rc = check_disk(&adl, attrs.label);
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
                if (*(attrs.label))
                {
                    fprintf(stdout,
                            "asmtool: Disk \"%s\" is marked an ASM disk, but does not match the label \"%s\"\n",
                            disk, attrs.label);
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

        case ASMTOOL_LIST_MANAGERS:
            fprintf(stderr, "asmtool: Can't get here (ASMTOOL_LIST_MANAGERS)!\n");
            break;

        default:
            fprintf(stderr, "Invalid operation\n");
            break;
    }

out_close:
    close(fd);

out:
    clear_attrs(&def_attrs);

    return 0;
}  /* main() */


