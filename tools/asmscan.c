/* Copyright (c) 2003 Oracle Corporation.  All rights reserved.  */

/*
 * The document contains proprietary information about Oracle Corporation.
 * It is provided under an agreement containing restrictions on use and
 * disclosure and is also protected by copyright law.
 * The information in this document is subject to change.
 */

/*
  NAME
    asmscan.c - Scan for Oracle Advanced Storage Management disks.
    
  DESCRIPTION
  
  This is the main source file for the ASM scan tool.
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
#include <libgen.h>
#include <errno.h>

#include <linux/types.h>
#include "oratypes.h"
#include "osmlib.h"
#include "linux/asmdisk.h"
#include "linux/asmmanager.h"

#include "list.h"


/*
 * Defines
 */
#define ASMFS_MAGIC     0x958459f6
#define DEV_PREFIX      "/dev/"




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
    const char *sd_name;
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
static int as_pattern_match(ASMScanPattern *pattern, const char *text);
static void as_clean_pattern_list(struct list_head *pattern_list);
static ASMScanDevice *as_device_new(struct list_head *device_list,
                                    const char *dev_name);
static void as_device_free(ASMScanDevice *device);
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


static int as_pattern_match(ASMScanPattern *pattern, const char *text)
{
    if (!pattern || !pattern->sp_pattern || !text)
        return -EINVAL;
    
    if (strnlen(text, pattern->sp_len + 1) < pattern->sp_len)
        return 0;
    if (strncmp(text, pattern->sp_pattern, pattern->sp_len))
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

    device->sd_name = dev_name;
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
        if (device->sd_path)
            free(device->sd_path);
        free(device);
    }
}  /* as_device_free() */



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

    {
        struct list_head *pos;
        ASMScanPattern *p;

        fprintf(stdout, "Order matches:\n");
        list_for_each(pos, &order_list) {
            p = list_entry(pos, ASMScanPattern, sp_list);
            fprintf(stdout, "    \"%s\"\n",
                    p->sp_pattern);
        }
        fprintf(stdout, "Exclude matches:\n");
        list_for_each(pos, &exclude_list) {
            p = list_entry(pos, ASMScanPattern, sp_list);
            fprintf(stdout, "    \"%s\"\n",
                    p->sp_pattern);
        }
    }

out:
    as_clean_pattern_list(&order_list);
    as_clean_pattern_list(&exclude_list);

    return 0;
}  /* main() */


