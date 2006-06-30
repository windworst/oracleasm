#ifndef KAPI_IMUTEX_H
#define KAPI_IMUTEX_H

#define mutex_lock down
#define mutex_unlock up
#define mutex_trylock(__m) (!down_trylock(__m))

#define i_mutex i_sem

#endif /* KAPI_IMUTEX_H */
