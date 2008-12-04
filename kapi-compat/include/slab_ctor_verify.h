#ifndef KAPI_SLAB_CTOR_VERIFY_H
#define KAPI_SLAB_CTOR_VERIFY_H

#include <linux/slab.h>

static void init_asmdisk_once(void *foo);
static void instance_init_once(void *foo);

static inline int slab_ctor_verify_check_flags(unsigned long flags)
{
	return (flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
		SLAB_CTOR_CONSTRUCTOR;
}

static void slab_ctor_verify_init_asmdisk_once(void *foo,
					       struct kmem_cache *cachep,
					       unsigned long flags)
{
	if (slab_ctor_verify_check_flags(flags))
		init_asmdisk_once(foo);
}
#define kapi_init_asmdisk_once slab_ctor_verify_init_asmdisk_once

static void slab_ctor_verify_instance_init_once(void *foo,
						struct kmem_cache *cachep,
						unsigned long flags)
{
	if (slab_ctor_verify_check_flags(flags))
		instance_init_once(foo);
}
#define kapi_instance_init_once slab_ctor_verify_instance_init_once

#endif
