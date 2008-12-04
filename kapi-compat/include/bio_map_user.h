#ifndef KAPI_BIO_MAP_USER_H
#define KAPI_BIO_MAP_USER_H

#include <linux/bio.h>

static struct bio *old_bio_map_user(struct request_queue *q, struct block_device *bdev,
                                    unsigned long uaddr, unsigned int len, int write_to_vm,
                                    gfp_t gfp_mask)
{
    return bio_map_user(q, bdev, uaddr, len, write_to_vm);
}
#define kapi_asm_bio_map_user old_bio_map_user

#endif
