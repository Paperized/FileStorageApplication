#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "icl_hash.h"
#include "file_stored.h"

typedef struct file_system file_system_t;

file_system_t* create_fs();

void set_policy_fs(file_system_t* fs, char* policy);

void free_fs(file_system_t* fs);

#endif