/*
 * NAME
 *	asmscan.c - Scan system for existing ASM disks.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      This tool scans existing block devices to determine which
 *      ones are already tagged for the Oracle Automatic Storage
 *      Management library.
 *
 * MODIFIED   (YYYY/MM/DD)
 *      2004/01/02 - Joel Becker <joel.becker@oracle.com>
 *              Initial LGPL header.
 *
 * Copyright (c) 2002-2004 Oracle Corporation.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License, version 2 as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have recieved a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
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
#define DEV_PREFIX      "/dev/"
#define PROC_IDE_FORMAT "/proc/ide/%s/media"




/*
 * Typedefs
 */
typedef struct _ASMScanPattern  ASMScanPattern;
typedef struct _ASMScanDevice   ASMScanDevice;


/*
 * Structures
 */
struct _ASMScanPattern
{
    struct list_head sp_list;
    struct list_head sp_matches;
    const char *sp_pattern;
    size_t sp_len;
};

struct _ASMScanDevice
{
    struct list_head sd_list;
    char *sd_name;
    char *sd_path;
};



/*
 * Function prototypes
 */
static inline size_t strnlen(const char *s, size_t size);
static void print_usage(int rc);
static void print_version();
static ASMScanPattern *as_pattern_new(struct list_head *pattern_list,
                                      const char *text);
static void as_pattern_free(ASMScanPattern *pattern);
static int as_pattern_match(ASMScanPattern *pattern,
                            const char *dev_name);
static void as_clean_pattern_list(struct list_head *pattern_list);
static ASMScanDevice *as_device_new(struct list_head *device_list,
                                    const char *dev_name);
static void as_device_free(ASMScanDevice *device);
static int as_exclude_device(struct list_head *exclude_list,
                             const char *dev_name);
static int as_order_device(struct list_head *order_list,
                           const char *dev_name);
static int as_ide_disk(const char *dev_name);
static int scan_devices(struct list_head *order_list,
                      struct list_head *exclude_list);
static int collect_asmtool(pid_t pid,
                           int out_fd[], int err_fd[],
                           char *output, int *outlen,
                           char *errput, int *errlen);
static void exec_asmtool(char *args[], int out_fd[], int err_fd[]);
static int run_asmtool(char *args[],
                       char **output, int *outlen,
                       char **errput, int *errlen);
static int needs_clean(const char *manager, const char *disk_name);
static int delete_disk(const char *manager, const char *disk_name);
static int clean_disks(const char *manager);
static int make_disks(const char *manager,
                      struct list_head *order_list);
static int is_manager(const char *filename);
static int parse_options(int argc, char *argv[], char **manager,
                         struct list_head *order_list,
                         struct list_head *exclude_list);



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
            "Usage: asmscan -l <manager> [-s <pattern> ...] [-x <pattern>]\n"
            "       asmscan -h\n"
            "       asmscan -V\n");
    exit(rc);
}  /* print_usage() */


static void print_version()
{
    fprintf(stderr, "asmscan version %s\n", VERSION);
    exit(0);
}  /* print_version() */


static ASMScanPattern *as_pattern_new(struct list_head *pattern_list,
                                      const char *text)
{
    ASMScanPattern *pattern;

    if (!text)
        return NULL;

    pattern = (ASMScanPattern *)malloc(sizeof(ASMScanPattern));
    if (!pattern)
        return NULL;

    INIT_LIST_HEAD(&pattern->sp_matches);

    pattern->sp_pattern = text;
    pattern->sp_len = strnlen(text, PATH_MAX);

    list_add_tail(&pattern->sp_list, pattern_list);

    return pattern;
}  /* as_pattern_new() */


static void as_pattern_free(ASMScanPattern *pattern)
{
    struct list_head *pos, *tmp;
    ASMScanDevice *device;

    if (pattern)
    {
        if (!list_empty(&pattern->sp_matches))
        {
            list_for_each_safe(pos, tmp, &pattern->sp_matches) {
                device = list_entry(pos, ASMScanDevice, sd_list);
                as_device_free(device);
            }
        }
        list_del(&pattern->sp_list);
        free(pattern);
    }
}  /* as_pattern_free() */


static int as_pattern_match(ASMScanPattern *pattern,
                            const char *dev_name)
{
    if (!pattern || !pattern->sp_pattern || !dev_name)
        return -EINVAL;
    
    if (strnlen(dev_name, pattern->sp_len + 1) < pattern->sp_len)
        return 0;
    if (strncmp(dev_name, pattern->sp_pattern, pattern->sp_len))
        return 0;

    return 1;
}  /* as_pattern_match() */


static void as_clean_pattern_list(struct list_head *pattern_list)
{
    struct list_head *pos, *tmp;
    ASMScanPattern *pattern;

    list_for_each_safe(pos, tmp, pattern_list) {
        pattern = list_entry(pos, ASMScanPattern, sp_list);

        /* Will list_del() */
        as_pattern_free(pattern);
    }
}  /* as_clean_pattern_list() */


static ASMScanDevice *as_device_new(struct list_head *device_list,
                                    const char *dev_name)
{
    int len;
    ASMScanDevice *device;

    device = (ASMScanDevice *)malloc(sizeof(ASMScanDevice));
    if (!device)
        return NULL;

    device->sd_name = strdup(dev_name);
    len = strlen(DEV_PREFIX) + strlen(device->sd_name) + 1;
    device->sd_path = (char *)malloc(sizeof(char) * len);
    if (!device->sd_path)
    {
        free(device);
        return NULL;
    }

    snprintf(device->sd_path, len, DEV_PREFIX "%s", device->sd_name);

    list_add_tail(&device->sd_list, device_list);

    return device;
}  /* as_device_new() */


static void as_device_free(ASMScanDevice *device)
{
    if (device)
    {
        list_del(&device->sd_list);
        if (device->sd_name)
            free(device->sd_name);
        if (device->sd_path)
            free(device->sd_path);
        free(device);
    }
}  /* as_device_free() */


static int as_exclude_device(struct list_head *exclude_list,
                             const char *dev_name)
{
    struct list_head *pos;
    ASMScanPattern *pattern;

    list_for_each(pos, exclude_list) {
        pattern = list_entry(pos, ASMScanPattern, sp_list);
        if (as_pattern_match(pattern, dev_name))
        {
            if (!as_device_new(&pattern->sp_matches, dev_name))
            {
                fprintf(stderr,
                        "asmscan: Error processing device \"%s\"\n",
                        dev_name);
            }
            return 1;
        }
    }

    return 0;
}  /* as_exclude_device() */


static int as_order_device(struct list_head *order_list,
                           const char *dev_name)
{
    struct list_head *pos;
    ASMScanPattern *pattern;

    list_for_each(pos, order_list) {
        pattern = list_entry(pos, ASMScanPattern, sp_list);
        if (as_pattern_match(pattern, dev_name))
        {
            if (!as_device_new(&pattern->sp_matches, dev_name))
            {
                fprintf(stderr,
                        "asmscan: Error processing device \"%s\"\n",
                        dev_name);
            }
            return 1;
        }
    }

    return 0;
}  /* as_order_device() */


static int as_ide_disk(const char *dev_name)
{
    FILE *f;
    int is_disk = 1;
    size_t len;
    char *proc_name;

    len = strlen(PROC_IDE_FORMAT) + strlen(dev_name);
    proc_name = (char *)malloc(sizeof(char) * len);
    if (!proc_name)
        return 0;

    snprintf(proc_name, len, PROC_IDE_FORMAT, dev_name);
    
    /* If not ide, file won't exist */
    f = fopen(proc_name, "r");
    if (f)
    {
        if (fgets(proc_name, len, f))
        {
            /* IDE devices we don't want to probe */
            if (!strncmp(proc_name, "cdrom", strlen("cdrom")) ||
                !strncmp(proc_name, "tape", strlen("tape")))
                is_disk = 0;
        }
        fclose(f);
    }

    free(proc_name);

    return is_disk;
}  /* as_ide_disk() */


/* Um, wow, this is, like, one big hardcode */
static int scan_devices(struct list_head *order_list,
                      struct list_head *exclude_list)
{
    int rc, major, minor;
    FILE *f;
    char *buffer, *name;

    buffer = (char *)malloc(sizeof(char) * (PATH_MAX + 1));
    if (!buffer)
        return -ENOMEM;

    name = (char *)malloc(sizeof(char) * (PATH_MAX + 1));
    if (!name)
    {
        free(buffer);
        return -ENOMEM;
    }

    f = fopen("/proc/partitions", "r");
    if (!f)
    {
        rc = -errno;
        goto out_free;
    }

    while (1)
    {
        rc = 0;
        if ((fgets(buffer, PATH_MAX + 1, f)) == NULL)
            break;

        name[0] = '\0';
        major = minor = 0;

        /* FIXME: If this is bad, send patches */
        if (sscanf(buffer, "%d %d %*d %99[^ \t\n]",
                   &major, &minor, name) < 3)
            continue;

        if (*name && major)
        {
            if (!as_ide_disk(name))
                continue;

            if (as_exclude_device(exclude_list, name))
                continue;

            if (!as_order_device(order_list, name))
            {
                fprintf(stderr,
                        "asmscan: Unable to order device \"%s\"!\n",
                        name);
            }
        }
    }

    fclose(f);

out_free:
    free(buffer);
    free(name);

    return rc;
}  /* scan_devices() */


static int collect_asmtool(pid_t pid,
                           int out_fd[], int err_fd[],
                           char *output, int *outlen,
                           char *errput, int *errlen)
{
    int status, rc, tot;
    pid_t wait_pid;

    while ((wait_pid = wait(&status)) != pid)
        ;

    close(out_fd[1]); close(err_fd[1]);

    tot = 0;
    while (1)
    {
        rc = read(out_fd[0], output + tot, *outlen - tot);
        if (!rc)
            break;
        else if (rc < 0)
        {
            if ((rc == EINTR) || (rc == EAGAIN))
                continue;
            goto out;
        }

        tot += rc;
    }
    output[*outlen > tot ? tot : *outlen - 1] = '\0';
    *outlen = tot;

    tot = 0;
    while (1)
    {
        rc = read(err_fd[0], errput + tot, *errlen - tot);
        if (!rc)
            break;
        else if (rc < 0)
        {
            if ((rc == EINTR) || (rc == EAGAIN))
                continue;
            goto out;
        }

        tot += rc;
    }
    errput[*errlen > tot ? tot : *errlen - 1] = '\0';
    *errlen = tot;

    if (!rc)
        rc = status;

out:
    close(out_fd[0]); close(err_fd[0]);
    return rc;
}  /* collect_asmtool() */


static void exec_asmtool(char *args[], int out_fd[], int err_fd[])
{
    int rc;

    args[0] = "asmtool";  /* FIXME: Do something pathlike */

    close(out_fd[0]); close(err_fd[0]);

    if (out_fd[1] != STDOUT_FILENO)
    {
        rc = dup2(out_fd[1], STDOUT_FILENO);
        if (rc < 0)
            _exit(-errno);
        close(out_fd[1]);
    }

    if (err_fd[1] != STDERR_FILENO)
    {
        rc = dup2(err_fd[1], STDERR_FILENO);
        if (rc < 0)
            _exit(-errno);
        close(err_fd[1]);
    }

    rc = execvp(args[0], args);
    /* Shouldn't get here */

    _exit(-errno);
}  /* exec_asmtool() */


static int run_asmtool(char *args[],
                       char **output, int *outlen,
                       char **errput, int *errlen)
{
    int rc;
    size_t put_size = getpagesize() * 8;
    int err_fd[2], out_fd[2];
    pid_t pid;

    /* Caller must leave args[0] NULL */
    if (!args || args[0])
        return -EINVAL;

    *output = (char *)malloc(sizeof(char) * put_size);
    if (!*output)
        return -ENOMEM;
    *outlen = put_size;

    *errput = (char *)malloc(sizeof(char) * put_size);
    if (!*errput)
    {
        free(*output);
        return -ENOMEM;
    }
    *errlen = put_size;

    rc = pipe(out_fd);
    if (rc)
    {
        free(*output); free(*errput);
        return -errno;
    }

    rc = pipe(err_fd);
    if (rc)
    {
        free(*output); free(*errput);
        close(out_fd[0]); close(out_fd[1]);
        return -errno;
    }

    pid = fork();
    if (pid < 0)
    {
        rc = -errno;
        free(*output); free(*errput);
        close(out_fd[0]); close(out_fd[1]);
        close(err_fd[0]); close(err_fd[1]);
        return rc;
    }

    /* These calls close the pipes */
    if (pid)
        rc = collect_asmtool(pid, out_fd, err_fd,
                             *output, outlen,
                             *errput, errlen);
    else
        exec_asmtool(args, out_fd, err_fd);

    return rc;
}  /* run_asmtool() */


static int needs_clean(const char *manager, const char *disk_name)
{
    int rc;
    char *output, *errput;
    int outlen, errlen;
    char * args[] =
    {
        NULL,  /* filled in later with the program */
        "-I",
        "-l",
        (char *)manager,
        "-n",
        (char *)disk_name,
        NULL
    };

    rc = run_asmtool(args, &output, &outlen, &errput, &errlen);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmscan: Error running asmtool on disk \"%s\": %s\n",
                disk_name, strerror(-rc));
    }
    else if (WIFEXITED(rc))
    {
        if (WEXITSTATUS(rc))
        {
            if (!strstr(errput, "Unable to"))
                return 1;
            if (strstr(errput, "No such device"))
                return 1;
            if (strstr(errput, "Invalid argument"))  /* Bad IDE lseek */
                return 1;
            fprintf(stderr,
                    "Error on %s (%d): %s\n",
                    disk_name, WEXITSTATUS(rc), errput);
        }
        else if (!strstr(output, "valid ASM disk"))
        {
            fprintf(stdout,
                    "Output on %s: %s\n",
                    disk_name, output);
            return 1;
        }
    }
    else if (WIFSIGNALED(rc))
    {
        fprintf(stderr,
                "asmscan: asmtool query of disk \"%s\" killed by signal %d\n",
                disk_name, WTERMSIG(rc));
    }
    else
    {
        fprintf(stderr,
                "asmscan: Unknown error running asmtool on disk \"%s\"\n",
                disk_name);
    }

    return 0;
}  /* needs_clean() */


static int delete_disk(const char *manager, const char *disk_name)
{
    int rc;
    char *output, *errput;
    int outlen, errlen;
    char * args[] =
    {
        NULL,  /* filled in later with the program */
        "-D",
        "-l",
        (char *)manager,
        "-n",
        (char *)disk_name,
        NULL
    };

    rc = run_asmtool(args, &output, &outlen, &errput, &errlen);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmscan: Error running asmtool on disk \"%s\": %s\n",
                disk_name, strerror(-rc));
    }
    else if (WIFEXITED(rc))
    {
        rc = WEXITSTATUS(rc);
        if (rc)
        {
            fprintf(stderr,
                    "Unable to delete invalid disk \"%s\": %s\n",
                    disk_name, strerror(-rc));
        }
    }
    else if (WIFSIGNALED(rc))
    {
        fprintf(stderr,
                "asmscan: asmtool delete of disk \"%s\" killed by signal %d\n",
                disk_name, WTERMSIG(rc));
        rc = -EINTR;
    }
    else
    {
        fprintf(stderr,
                "asmscan: Unknown error running asmtool on disk \"%s\"\n",
                disk_name);
    }

    return rc;
}  /* delete_disk() */


static int clean_disks(const char *manager)
{
    int rc;
    char *disk_path;
    DIR *dir;
    struct dirent *d_ent;

    disk_path = asm_disk_path(manager, "");
    if (!disk_path)
        return -ENOMEM;

    dir = opendir(disk_path);
    rc = -errno;
    free(disk_path);
    if (!dir)
        goto out;

    while ((d_ent = readdir(dir)) != NULL)
    {
        if (!strcmp(d_ent->d_name, ".") || !strcmp(d_ent->d_name, ".."))
            continue;

        rc = needs_clean(manager, d_ent->d_name);
        if (!rc)
            continue;

        fprintf(stdout, "Cleaning disk \"%s\"\n", d_ent->d_name);
        rc = delete_disk(manager, d_ent->d_name);
    }

out:
    return rc;
}  /* clean_disks() */

static int device_is_disk(const char *manager, const char *device,
                          char **disk_name)
{
    int rc;
    char *output, *errput, *ptr;
    int outlen, errlen;
    char * args[] =
    {
        NULL,  /* filled in later with the program */
        "-I",
        "-l",
        (char *)manager,
        "-n",
        (char *)device,
        "-a",
        "label",
        NULL
    };

    rc = run_asmtool(args, &output, &outlen, &errput, &errlen);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmscan: Error running asmtool on device \"%s\": %s\n",
                device, strerror(-rc));
    }
    else if (WIFEXITED(rc))
    {
        if (WEXITSTATUS(rc))
        {
            if (strstr(errput, "not marked"))
                return 0;
            if (strstr(errput, "No such"))
                return 0;
            if (strstr(errput, "Invalid argument"))  /* Bad IDE lseek */
                return 0;
            fprintf(stderr,
                    "asmscan: Error on %s (%d): %s\n",
                    device, WEXITSTATUS(rc), errput);
        }
        else
        {
            if (strstr(output, "is marked"))
            {
                ptr = strrchr(output, '"');
                if (!ptr)
                    return 0;  /* Should error */
                *ptr = '\0';
                ptr = strrchr(output, '"');
                if (!ptr)
                    return 0;  /* Again, error */
                *disk_name = strdup(ptr + 1);
                return 1;
            }
            
            fprintf(stdout,
                    "Output on %s: %s\n",
                    device, output);
        }
    }
    else if (WIFSIGNALED(rc))
    {
        fprintf(stderr,
                "asmscan: asmtool query of device \"%s\" killed by signal %d\n",
                device, WTERMSIG(rc));
    }
    else
    {
        fprintf(stderr,
                "asmscan: Unknown error running asmtool on device \"%s\"\n",
                device);
    }

    return 0;
}  /* device_is_disk() */


static int instantiate_disk(const char *manager,
                            const char *disk_name,
                            const char *device)
{
    int rc;
    char *output, *errput;
    int outlen, errlen;
    char * args[] =
    {
        NULL,  /* filled in later with the program */
        "-C",
        "-l",
        (char *)manager,
        "-n",
        (char *)disk_name,
        "-s",
        (char *)device,
        "-a",
        "mark=no",
        NULL
    };

    rc = run_asmtool(args, &output, &outlen, &errput, &errlen);
    if (rc < 0)
    {
        fprintf(stderr,
                "asmscan: Error running asmtool to create disk \"%s\": %s\n",
                disk_name, strerror(-rc));
    }
    else if (WIFEXITED(rc))
    {
        rc = WEXITSTATUS(rc);
        if (rc)
        {
            if (!strstr(errput, "File exists"))
            {
                fprintf(stderr,
                        "asmscan: Unable to create disk \"%s\": %s\n",
                        disk_name, strerror(-rc));
            }
            else
                rc = 0;
        }
    }
    else if (WIFSIGNALED(rc))
    {
        fprintf(stderr,
                "asmscan: asmtool creation of disk \"%s\" killed by signal %d\n",
                disk_name, WTERMSIG(rc));
        rc = -EINTR;
    }
    else
    {
        fprintf(stderr,
                "asmscan: Unknown error running asmtool on disk \"%s\"\n",
                disk_name);
    }

    return rc;
}


static int make_disks(const char *manager, struct list_head *order_list)
{
    int rc;
    char *disk_name;
    struct list_head *pos_l, *pos_d;
    ASMScanPattern *pattern;
    ASMScanDevice *device;

    list_for_each(pos_l, order_list) {
        pattern = list_entry(pos_l, ASMScanPattern, sp_list);
        list_for_each(pos_d, &pattern->sp_matches) {
            device = list_entry(pos_d, ASMScanDevice, sd_list);
            rc = device_is_disk(manager, device->sd_path, &disk_name);
            if (rc)
            {
                fprintf(stdout,
                        "Instantating disk \"%s\"\n", disk_name);
                rc = instantiate_disk(manager, disk_name,
                                      device->sd_path);
            }
        }
    }

    return 0;
}  /* make_disks() */


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


extern char *optarg;
extern int optopt;
extern int opterr;
static int parse_options(int argc, char *argv[], char **manager,
                         struct list_head *order_list,
                         struct list_head *exclude_list)
{
    int c;

    opterr = 0;
    while((c = getopt(argc, argv, ":hVl:s:x:")) != EOF)
    {
        switch(c)
        {
            case 'h':
                print_usage(0);
                break;

            case 'V':
                print_version();
                break;

            case 'l':
                if (!optarg || !*optarg)
                    return -EINVAL;
                *manager = optarg;
                break;

            case 's':
                if (!optarg || !*optarg)
                    return -EINVAL;
                if (!as_pattern_new(order_list, optarg))
                {
                    fprintf(stderr, 
                            "asmscan: Error initializing pattern \"%s\"\n",
                            optarg);
                }
                break;

            case 'x':
                if (!optarg || !*optarg)
                    return -EINVAL;
                if (!as_pattern_new(exclude_list, optarg))
                {
                    fprintf(stderr, 
                            "asmscan: Error initializing pattern \"%s\"\n",
                            optarg);
                }
                break;

            case '?':
                fprintf(stderr, "asmscan: Invalid option: \'-%c\'\n",
                        optopt);
                return -EINVAL;
                break;

            case ':':
                fprintf(stderr,
                        "asmscan: Option \'-%c\' requires an argument\n",
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



/*
 * Main program
 */
int main(int argc, char *argv[])
{
    int rc;
    char *manager = NULL;
    struct list_head order_list, exclude_list;

    INIT_LIST_HEAD(&order_list);
    INIT_LIST_HEAD(&exclude_list);

    rc = parse_options(argc, argv, &manager,
                       &order_list, &exclude_list);
    if (rc)
        print_usage(rc);

    if (!as_pattern_new(&order_list, ""))
    {
        fprintf(stderr,
                "asmscan: Unable to create the default pattern\n");
        return(-ENOMEM);
    }

    rc = -EINVAL;
    if (!manager || !*manager)
    {
        fprintf(stderr, "You must specify a manager.\n");
        print_usage(rc);
    }

    rc = is_manager(manager);
    if (rc)
    {
        fprintf(stderr, "Error opening manager: %s\n",
                strerror(-rc));
        goto out;
    }

    rc = clean_disks(manager);
    if (rc)
        goto out;

    rc = scan_devices(&order_list, &exclude_list);
    if (rc)
        goto out;

    rc = make_disks(manager, &order_list);

out:
    as_clean_pattern_list(&order_list);
    as_clean_pattern_list(&exclude_list);

    return rc;
}  /* main() */


