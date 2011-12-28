#ifndef KAPI_BLKDEV_GET_PUT_H
#define KAPI_BLKDEV_GET_PUT_H

#include <linux/fs.h>

static int kapi_asm_blkdev_get(struct block_device *bdev, fmode_t mode, void *holder)
{
	int ret;

#if defined(BLKDEV_GET_THREE)
	ret = blkdev_get(bdev, mode, 0);
#else
	ret = blkdev_get(bdev, mode);
#endif

#if defined(BD_CLAIM)
	bd_claim(bdev, holder);
#endif
	return ret;
}

static int kapi_asm_blkdev_put(struct block_device *bdev, fmode_t mode)
{
#if defined(BD_CLAIM)
	bd_release(bdev);
#endif

#if defined(BLKDEV_PUT_TWO)
	return blkdev_put(bdev, mode);
#else
	return blkdev_put(bdev);
#endif

}

#endif
