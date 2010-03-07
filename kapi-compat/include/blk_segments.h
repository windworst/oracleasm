#ifndef KAPI_BLK_SEGMENTS_H
#define KAPI_BLK_SEGMENTS_H

#define queue_max_segments(a)	min(queue_max_hw_segments(a), queue_max_phys_segments(a))

#endif
