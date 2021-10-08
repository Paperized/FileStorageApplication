#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef enum bool { FALSE, TRUE } bool_t;

#define START_RED_CONSOLE "\033[31m"
#define START_YELLOW_CONSOLE "\033[33m"
#define RESET_COLOR_CONSOLE "\033[0m\n"

#define PRINT_WITH_COLOR(color, title, errno_code, message, ...)  printf(color "[" title "] " message " %s::%s:%d [%s]\n" RESET_COLOR_CONSOLE, ##__VA_ARGS__, (__FILE__), __func__, __LINE__, strerror(errno_code));
#define PRINT_WARNING(errno_code, message, ...)  PRINT_WITH_COLOR(START_YELLOW_CONSOLE, "Warning", errno_code, message, ## __VA_ARGS__)
#define PRINT_ERROR(errno_code, message, ...)  PRINT_WITH_COLOR(START_RED_CONSOLE, "Error", errno_code, message, ## __VA_ARGS__)
#define PRINT_FATAL(errno_code, message, ...)  PRINT_WITH_COLOR(START_RED_CONSOLE, "Fatal", errno_code, message, ## __VA_ARGS__)
#define PRINT_INFO(message, ...)     printf("[Info] " message "\n", ## __VA_ARGS__)

#ifdef DEBUG_LOG
    #define PRINT_INFO_DEBUG(message, ...) PRINT_INFO(message, ## __VA_ARGS__)
    #define PRINT_WARNING_DEBUG(errno_code, message, ...)  PRINT_WARNING(errno_code, message, ## __VA_ARGS__)
    #define PRINT_ERROR_DEBUG(errno_code, message, ...)  PRINT_ERROR(errno_code, message, ## __VA_ARGS__)
    #define PRINT_FATAL_DEBUG(errno_code, message, ...)  PRINT_FATAL(errno_code, message, ## __VA_ARGS__)
#else
    #define PRINT_INFO_DEBUG(message, ...)
    #define PRINT_WARNING_DEBUG(errno_code, message, ...)
    #define PRINT_ERROR_DEBUG(errno_code, message, ...)
    #define PRINT_FATAL_DEBUG(errno_code, message, ...)
#endif

#define CHECK_FATAL_ERRNO(var, value, ...) var = value; \
                                            if(errno == ENOMEM) { \
                                                PRINT_FATAL(errno, __VA_ARGS__); \
                                                exit(EXIT_FAILURE); \
                                            }

#define CHECK_FATAL_EQ(var, value, err, ...) if((var = value) == err) { \
                                                PRINT_FATAL(errno, __VA_ARGS__); \
                                                exit(EXIT_FAILURE); \
                                            }

#define CHECK_FATAL_EVAL(expr, ...)     if(expr) { \
                                                PRINT_FATAL(errno, __VA_ARGS__); \
                                                exit(EXIT_FAILURE); \
                                            }

#define CHECK_ERROR_EQ(var, value, err, ret_value, ...) if((var = value) == err) { \
                                                            PRINT_ERROR(errno, __VA_ARGS__); \
                                                            return ret_value; \
                                                        }

#define CHECK_ERROR_NEQ(var, value, err, ret_value, ...) if((var = value) != err) { \
                                                            PRINT_ERROR(errno, __VA_ARGS__); \
                                                            return ret_value; \
                                                        }

#define CHECK_ERROR_EQ_ERRNO(var, value, err, ret_value, errno_val, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT_ERROR(errno_val, __VA_ARGS__); \
                                                                            return ret_value; \
                                                                        }

#define CHECK_WARNING_EQ_ERRNO(var, value, err, ret_value, errno_val, message, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT_WARNING(errno_val, message, __VA_ARGS__); \
                                                                            return ret_value; \
                                                                        }

#define CHECK_WARNING_NEQ_ERRNO(var, value, err, ret_value, errno_val, message, ...) if((var = value) != err) { \
                                                                            errno = errno_val; \
                                                                            PRINT_WARNING(errno_val, message, __VA_ARGS__); \
                                                                            return ret_value; \
                                                                        }

#define RET_IF(cond, val) if(cond) return (val)
#define NRET_IF(cond) if(cond) return

#define INIT_MUTEX(m) CHECK_FATAL_EVAL(pthread_mutex_init(m, NULL) != 0, "Mutex init failed!")
#define INIT_RWLOCK(rw) CHECK_FATAL_EVAL(pthread_rwlock_init(rw, NULL) != 0, "RWLock init failed!")
#define INIT_COND(cond) CHECK_FATAL_EVAL(pthread_cond_init(cond, NULL) != 0, "Condition variable init failed!")

#define WLOCK_RWLOCK(rw) CHECK_FATAL_EVAL(pthread_rwlock_wrlock(rw) != 0, "RWLock Wlock failed!")
#define RLOCK_RWLOCK(rw) CHECK_FATAL_EVAL(pthread_rwlock_rdlock(rw) != 0, "RWLock Rlock failed!")
#define UNLOCK_RWLOCK(rw) CHECK_FATAL_EVAL(pthread_rwlock_unlock(rw) != 0, "RWLock unlock failed!")

#define LOCK_MUTEX(m) CHECK_FATAL_EVAL(pthread_mutex_lock(m) != 0, "Mutex lock failed!")
#define UNLOCK_MUTEX(m) CHECK_FATAL_EVAL(pthread_mutex_unlock(m) != 0, "Mutex unlock failed!")

#define COND_SIGNAL(s) CHECK_FATAL_EVAL(pthread_cond_signal(s) != 0, "Condition variable signal failed!")
#define COND_BROADCAST(s) CHECK_FATAL_EVAL(pthread_cond_broadcast(s) != 0, "Condition variable signal failed!")
#define COND_WAIT(s, m) CHECK_FATAL_EVAL(pthread_cond_wait(s, m) != 0, "Condition variable wait failed!")

#define MAKE_COPY(name, type, from) CHECK_FATAL_EQ(name, malloc(sizeof(type)), NULL, NO_MEM_FATAL); \
                                    memcpy(name, &(from), sizeof(type))

#define MAKE_COPY_BYTES(name, size, from) CHECK_FATAL_EQ(name, malloc(size), NULL, NO_MEM_FATAL); \
                                            strncpy(name, from, size)

#define SET_VAR_MUTEX(var, expr, m)     LOCK_MUTEX(m); \
                                        var = expr; \
                                        UNLOCK_MUTEX(m)

#define GET_VAR_MUTEX(expr, output, m) LOCK_MUTEX(m); \
                                      output = expr; \
                                      UNLOCK_MUTEX(m)

#define SET_VAR_RWLOCK(var, expr, m)     WLOCK_RWLOCK(m); \
                                        var = expr; \
                                        UNLOCK_RWLOCK(m)

#define GET_VAR_RWLOCK(expr, output, m) RLOCK_RWLOCK(m); \
                                      output = expr; \
                                      UNLOCK_RWLOCK(m)

#define EXEC_WITH_MUTEX(istr, m) LOCK_MUTEX(m); \
                                        istr; \
                                        UNLOCK_MUTEX(m)

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

int read_file_util(const char* pathname, void** buffer, size_t* size);
int write_file_util(const char* pathname, void* buffer, size_t size);
int append_file_util(const char* pathname, void* buffer, size_t size);

char* get_filename_from_path(const char* path, size_t path_len, size_t* filename_len);
int buildpath(char* dest, const char* src1, const char* src2, size_t src1length, size_t src2length);
int filesize_string_to_byte(char* str, unsigned int max_length);

#define NO_MEM_FATAL "Cannot allocate more memory!"
#define THREAD_CREATE_FATAL "Cannot create new thread!"

#endif