#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef enum bool { FALSE, TRUE } bool_t;

#define START_RED_CONSOLE "\033[31m"
#define START_YELLOW_CONSOLE "\033[33m"
#define RESET_COLOR_CONSOLE "\033[0m\n"

#define PRINT_FATAL(errno_code, ...) printf(START_RED_CONSOLE "[Fatal] "); \
                            printf(__VA_ARGS__); \
                            printf(" %s::%s:%d [%s]" RESET_COLOR_CONSOLE, __FILE__, __func__, __LINE__, strerror(errno_code))

#define PRINT_ERROR(errno_code, ...) printf(START_RED_CONSOLE "[Error] "); \
                            printf(__VA_ARGS__); \
                            printf(" %s::%s:%d [%s]" RESET_COLOR_CONSOLE, __FILE__, __func__, __LINE__, strerror(errno_code))

#define PRINT_WARNING(errno_code, ...) printf(START_YELLOW_CONSOLE "[Warning] "); \
                            printf(__VA_ARGS__); \
                            printf(" %s::%s:%d [%s]" RESET_COLOR_CONSOLE, __FILE__, __func__, __LINE__, strerror(errno_code))

#define PRINT_INFO(...)     printf("[Info] "); \
                            printf(__VA_ARGS__)
  
#define CHECK_FATAL_ERRNO(var, value, ...) var = value; \
                                            if(errno == ENOMEM) { \
                                                PRINT_FATAL(errno, __VA_ARGS__); \
                                                exit(EXIT_FAILURE); \
                                            }

#define CHECK_FATAL_EQ(var, value, err, ...) if((var = value) == err) { \
                                                PRINT_FATAL(errno, __VA_ARGS__); \
                                                exit(EXIT_FAILURE); \
                                            }

#define CHECK_ERROR_EQ(var, value, err, ret_value, ...) if((var = value) == err) { \
                                                            PRINT_ERROR(errno, __VA_ARGS__); \
                                                            return ret_value; \
                                                        }

#define CHECK_ERROR_EQ_ERRNO(var, value, err, ret_value, errno_val, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT_ERROR(errno_val, __VA_ARGS__); \
                                                                            return ret_value; \
                                                                        }
                                
#define CHECK_WARNING_EQ_ERRNO(var, value, err, ret_value, errno_val, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT_WARNING(errno_val, __VA_ARGS__); \
                                                                            return ret_value; \
                                                                        }


#define LOCK_MUTEX(m) pthread_mutex_lock(m);

#define DLOCK_MUTEX(m) LOCK_MUTEX(m); \
                        printf("Lockato in riga: %d in %s Mutex: %s", __LINE__, __FILE__, (char*)#m); \
                        printf(".\n")

#define UNLOCK_MUTEX(m) pthread_mutex_unlock(m);

#define DUNLOCK_MUTEX(m) UNLOCK_MUTEX(m); \
                        printf("Unlockato in riga: %d in %s Mutex: %s", __LINE__, __FILE__, (char*)#m); \
                        printf(".\n")

#define SET_VAR_MUTEX(var, expr, m)  pthread_mutex_lock(m); \
                                        var = expr; \
                                        pthread_mutex_unlock(m)

#define GET_VAR_MUTEX(expr, output, m) pthread_mutex_lock(m); \
                                      output = expr; \
                                      pthread_mutex_unlock(m)

#define EXEC_WITH_MUTEX(istr, m) pthread_mutex_lock(m); \
                                        istr; \
                                        pthread_mutex_unlock(m)

#define MIN(x, y) x < y ? x : y

int read_file_util(const char* pathname, void** buffer, size_t* size);
int write_file_util(const char* pathname, void* buffer, size_t size);
int append_file_util(const char* pathname, void* buffer, size_t size);

void extract_dirname_and_filename(const char* fullpath, char** dir, char** fn);
char* buildpath(char* src1, const char* src2, size_t src1length, size_t src2length);

#define NO_MEM_FATAL "Cannot allocate more memory!"

#endif