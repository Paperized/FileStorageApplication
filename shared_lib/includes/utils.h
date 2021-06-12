#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>

typedef enum bool { FALSE, TRUE } bool_t;

#define LOCK_MUTEX(m) pthread_mutex_lock(m)
                        //printf("Lockato in riga: %d in %s ", __LINE__, __FILE__); \
                        //printf(".\n")

#define UNLOCK_MUTEX(m) pthread_mutex_unlock(m)
                        //printf("Unlockato in riga: %d in %s ", __LINE__, __FILE__); \
                        //printf(".\n")

#define SET_VAR_MUTEX(var, value, m)  pthread_mutex_lock(m); \
                                        var = value; \
                                        pthread_mutex_unlock(m)

#define GET_VAR_MUTEX(var, output, m) pthread_mutex_lock(m); \
                                      output = var; \
                                      pthread_mutex_unlock(m)

#define EXEC_WITH_MUTEX(var, m) pthread_mutex_lock(m); \
                                        var; \
                                        pthread_mutex_unlock(m)

int read_file_util(const char* pathname, void** buffer, size_t* size);
int write_file_util(const char* pathname, void* buffer, size_t size);
int append_file_util(const char* pathname, void* buffer, size_t size);

void extract_dirname_and_filename(const char* fullpath, char** dir, char** fn);

#endif