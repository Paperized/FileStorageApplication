#ifndef __REPLACEMENT_POLICY__
#define __REPLACEMENT_POLICY__

#include "server.h"

// Handles the replacement when the capacity is missing, a pathname can be specified as the first parameter to skip a 
// certain file that should not be deleted (E.g. the file you want to add or update).
// If the memory needed is not specified (= 0) then the first file will be deleted using the current algorithm
bool_t run_replacement_algorithm(const char* skip_file, size_t mem_needed, linked_list_t** output);

// Used in the sorting if FIFO
int replacement_policy_fifo(const void* f1_ptr, const void* f2_ptr);
// Used in the sorting if LFU
int replacement_policy_lfu(const void* f1_ptr, const void* f2_ptr);
// Used in the sorting if LRU
int replacement_policy_lru(const void* f1_ptr, const void* f2_ptr);

#endif