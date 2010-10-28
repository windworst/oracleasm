#ifndef KAPI_CLEAR_INODE_H
#define KAPI_CLEAR_INODE_H

#define evict_inode		clear_inode
#define end_writeback(a)	do {} while(0);

#endif /* KAPI_CLEAR_INODE_H */
