#include "replacement_policy.h"

// 0 == equal, > 0 t1 greater, < 0 t1 less
static int cmp_time(struct timespec* t1, struct timespec* t2)
{
    if(t1->tv_sec != t2->tv_sec)
        return t1->tv_sec - t2->tv_sec;

    return t1->tv_nsec - t2->tv_nsec;
}

int replacement_policy_fifo(file_stored_t* f1, file_stored_t* f2)
{
    return cmp_time(file_get_creation_time(f1), file_get_creation_time(f2));
}

int replacement_policy_lru(file_stored_t* f1, file_stored_t* f2)
{
    return cmp_time(file_get_last_use_time(f1), file_get_last_use_time(f2));
}

int replacement_policy_lfu(file_stored_t* f1, file_stored_t* f2)
{
    return file_get_use_frequency(f1) - file_get_use_frequency(f2);
}