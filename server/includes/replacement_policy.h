#ifndef __REPLACEMENT_POLICY__
#define __REPLACEMENT_POLICY__

#include "server.h"

bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output);

int replacement_policy_fifo(const void* f1_ptr, const void* f2_ptr);
int replacement_policy_lfu(const void* f1_ptr, const void* f2_ptr);
int replacement_policy_lru(const void* f1_ptr, const void* f2_ptr);

#endif