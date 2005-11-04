/*
 * NAME
 *	devtype.c - Scan system device types.
 *
 * AUTHOR
 * 	Joel Becker <joel.becker@oracle.com>
 *
 * DESCRIPTION
 *      Simple code to check Linux device types.
 *
 * MODIFIED   (YYYY/MM/DD)
 *      2005/10/24 - Joel Becker <joel.becker@oracle.com>
 *              This file created.
 *
 * Copyright (c) 2002-2005 Oracle Corporation.  All rights reserved.
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#include "devmap.h"

#define CHAR_TEXT "Character devices:"
#define BLOCK_TEXT "Block devices:"

struct devmap {
    char *m_name;
    unsigned int m_major;
    struct devmap *m_next;
};

static struct devmap *char_list = NULL;
static struct devmap *block_list = NULL;

static void free_major_map_list(struct devmap **map)
{
    struct devmap *tmp;
    while (*map)
    {
        tmp = *map;
        *map = tmp->m_next;

        free(tmp->m_name);
        free(tmp);
    }
}

static void free_major_map(void)
{
    free_major_map_list(&char_list);
    free_major_map_list(&block_list);
}

static int load_major_map(void)
{
    static int loaded = 0;
    int rc;
    unsigned int major;
    char *buffer;
    char *name;
    struct devmap *map;
    struct devmap **list = NULL;
    FILE *f;

    if (loaded)
        return 0;
    
    buffer = (char *)malloc(sizeof(char) * (PATH_MAX + 1));
    if (!buffer)
        return -ENOMEM;

    name = (char *)malloc(sizeof(char) * (PATH_MAX + 1));
    if (!name)
    {
        free(buffer);
        return -ENOMEM;
    }

    f = fopen64("/proc/devices", "r");
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

        if (!strncmp(buffer, CHAR_TEXT, strlen(CHAR_TEXT)))
        {
            list = &char_list;
            continue;
        }

        if (!strncmp(buffer, BLOCK_TEXT, strlen(BLOCK_TEXT)))
        {
            list = &block_list;
            continue;
        }

        name[0] = '\0';
        major = 0;

        /* If /proc/devices changes, fix */
        if (sscanf(buffer, "%u %99[^ \t\n]", &major, name) < 2)
            continue;

        if (!list)
            continue;

        //fprintf(stdout, "Device %s: %u\n", name, major);
        map = malloc(sizeof(struct devmap));
        if (!map)
        {
            rc = -ENOMEM;
            goto out_free_list;
        }
        map->m_name = strdup(name);
        if (!map->m_name)
        {
            rc = -ENOMEM;
            goto out_free_list;
        }
        map->m_major = major;
        map->m_next = *list;
        *list = map;
    }

    fclose(f);

    loaded = 1;

out_free:
    free(name);
    free(buffer);

    return rc;

out_free_list:
    free_major_map();
    goto out_free;
}

static const char *lookup_device(struct devmap *map, unsigned int major)
{
    while (map)
    {
        if (map->m_major == major)
            return map->m_name;

        map = map->m_next;
    }

    return NULL;
}

const char *lookup_char_device(unsigned int major)
{
    if (load_major_map())
        return NULL;

    return lookup_device(char_list, major);
}

const char *lookup_block_device(unsigned int major)
{
    if (load_major_map())
        return NULL;

    return lookup_device(block_list, major);
}

int device_is_char(int fd, const char *type)
{
    int rc;
    struct stat64 stat_buf;
    const char *test_type;

    rc = fstat64(fd, &stat_buf);
    if (rc)
        return 0;

    if (!S_ISCHR(stat_buf.st_mode))
        return 0;

    test_type = lookup_char_device(major(stat_buf.st_rdev));
    if (test_type && !strcmp(type, test_type))
        return 1;

    return 0;
}

int device_is_block(int fd, const char *type)
{
    int rc;
    struct stat64 stat_buf;
    const char *test_type;

    rc = fstat64(fd, &stat_buf);
    if (rc)
        return 0;

    if (!S_ISBLK(stat_buf.st_mode))
        return 0;

    test_type = lookup_block_device(major(stat_buf.st_rdev));
    if (test_type && !strcmp(type, test_type))
        return 1;

    return 0;
}

#ifdef DEBUG_EXE
int main(int argc, char *argv[])
{
    int rc, i; 
    char *name;

    rc = load_major_map();
    if (rc)
        perror("foo");
    else
    {
        for (i = 1; i < argc; i++)
        {
            name = lookup_block_device(atoi(argv[i]));
            if (name)
                fprintf(stdout, "%s:%20s\n", name, argv[i]);
        }
    }

    free_major_map();

    return rc;
}
#endif  /* DEBUG_EXE */
