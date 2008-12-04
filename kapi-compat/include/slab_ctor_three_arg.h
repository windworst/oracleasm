#ifndef KAPI_SLAB_CTOR_THREE_ARG_H
#define KAPI_SLAB_CTOR_THREE_ARG_H

#include <linux/slab.h>

static void init_asmdisk_once(void *foo);
static void instance_init_once(void *foo);

static void slab_ctor_three_arg_init_asmdisk_once(void *foo,
						  struct kmem_cache *cachep,
						  unsigned long flags)
{
	init_asmdisk_once(foo);
}
#define kapi_init_asmdisk_once slab_ctor_three_arg_init_asmdisk_once

static void slab_ctor_three_arg_instance_init_once(void *foo,
						   struct kmem_cache *cachep,
						   unsigned long flags)
{
	instance_init_once(foo);
}
#define kapi_instance_init_once slab_ctor_three_arg_instance_init_once

#endif
