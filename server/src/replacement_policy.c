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
    return cmp_time(&f1->creation_time, &f2->creation_time);
}

int replacement_policy_lru(file_stored_t* f1, file_stored_t* f2)
{
    return cmp_time(&f1->last_use_time, &f2->last_use_time);
}

int replacement_policy_lfu(file_stored_t* f1, file_stored_t* f2)
{
    return f1->use_frequency - f2->use_frequency;
}