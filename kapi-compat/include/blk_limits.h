#ifndef KAPI_BLK_LIMITS_H
#define KAPI_BLK_LIMITS_H

#define queue_max_sectors(a)		((a)->max_sectors)
#define queue_max_phys_segments(a)	((a)->max_phys_segments)
#define queue_max_hw_segments(a)	((a)->max_hw_segments)
#define bdev_physical_block_size(a)	bdev_hardsect_size(a)

#endif
