#ifndef __REPLACEMENT_POLICY__
#define __REPLACEMENT_POLICY__

#include "server.h"

int replacement_policy_fifo(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lfu(file_stored_t* file1, file_stored_t* file2);
int replacement_policy_lru(file_stored_t* file1, file_stored_t* file2);

#endif