#ifndef KAPI_BIO_END_IO_H
#define KAPI_BIO_END_IO_H

#include <linux/bio.h>

static void asm_end_bio_io(struct bio *bio, int error);

static int old_end_bio_io(struct bio *bio, unsigned int bytes_done,
			  int error)
{
	if (bio->bi_size)
		return 1;

	asm_end_bio_io(bio, error);
	return 0;
}
#define kapi_asm_end_bio_io old_end_bio_io

#endif
