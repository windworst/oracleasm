#ifndef KAPI_SLAB_CTOR_TWO_ARG_H
#define KAPI_SLAB_CTOR_TWO_ARG_H

#include <linux/slab.h>

static void init_asmdisk_once(void *foo);
static void instance_init_once(void *foo);

static void slab_ctor_two_arg_init_asmdisk_once(struct kmem_cache *cachep,
						void *foo)
{
	init_asmdisk_once(foo);
}
#define kapi_init_asmdisk_once slab_ctor_two_arg_init_asmdisk_once

static void slab_ctor_two_arg_instance_init_once(struct kmem_cache *cachep,
						 void *foo)
{
	instance_init_once(foo);
}
#define kapi_instance_init_once slab_ctor_two_arg_instance_init_once

#endif
