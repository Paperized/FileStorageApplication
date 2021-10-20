#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "server_api_utils.h"

// If APP_NAME is defined by a program we concatenate it with "-" otherwise we get and empty string
// The final macro __APP_NAME it's used to have a customized log header name
// For example to print [Server-Info] for the server and [Client-Info] for the client, [Info] if no APP_NAME was defined
#if !defined(APP_NAME)
    #define APP_NAME ""
    #define __APP_NAME ""
#else
    #define __APP_NAME APP_NAME "-"
#endif

typedef enum bool { FALSE, TRUE } bool_t;

// Colors used for different Log levels
#define START_RED_CONSOLE "\033[31m"
#define START_YELLOW_CONSOLE "\033[33m"
#define RESET_COLOR_CONSOLE "\033[0m\n"

// Print with color it's a base macro with takes various parameters such as color, the title, a message and arguments
// It prints also the file, the function and the line where the print happened and also a errno string value
#define PRINT_WITH_COLOR(color, title, errno_code, message, ...)  printf(color "[" __APP_NAME title "] " message " %s::%s:%d [%s]\n" RESET_COLOR_CONSOLE, ##__VA_ARGS__, (__FILE__), __func__, __LINE__, strerror(errno_code));

// Implementation of various log levels
#define PRINT_WARNING(errno_code, message, ...)  PRINT_WITH_COLOR(START_YELLOW_CONSOLE, "Warning", errno_code, message, ## __VA_ARGS__)
#define PRINT_ERROR(errno_code, message, ...)  PRINT_WITH_COLOR(START_RED_CONSOLE, "Error", errno_code, message, ## __VA_ARGS__)
#define PRINT_FATAL(errno_code, message, ...)  PRINT_WITH_COLOR(START_RED_CONSOLE, "Fatal", errno_code, message, ## __VA_ARGS__)
#define PRINT_INFO(message, ...)     printf("[" __APP_NAME "Info] " message "\n", ## __VA_ARGS__)

#define DEBUG_LOG

// Used to print debug messages if enabled
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

// Make a copy of a variable of certain type
// The var it's malloc'ed and initialized with the from variable
#define MAKE_COPY(name, type, from) CHECK_FATAL_EQ(name, malloc(sizeof(type)), NULL, NO_MEM_FATAL); \
                                    memcpy(name, &(from), sizeof(type))

// Make a copy of a variable of a certain size
// The var it's malloc'ed and initialized with the from variable
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

// Read data from a file located in pathname
int read_file_util(const char* pathname, void** buffer, size_t* size);

// Write to a file located in pathname a buffer with a certain size
int write_file_util(const char* pathname, void* buffer, size_t size);

// Append to a file located in pathname a buffer with a certain size
int append_file_util(const char* pathname, void* buffer, size_t size);

// Get the filename from a path in input, returns a pointer to the first letter of the filename and a size of the filename
// The return value is not malloc'ed and should not be freed by itself
char* get_filename_from_path(const char* path, size_t path_len, size_t* filename_len);

// From src1 and src2 build a file path inside dest
int buildpath(char* dest, const char* src1, const char* src2, size_t src1length, size_t src2length);

// Convert a string which contains the size and the unit measure of a file to bytes
// E.g. 300KB => 300000, 10B => 10, 1MB => 1000000
int filesize_string_to_byte(char* str, unsigned int max_length);

// Check whether an operation is a valid one for the server
bool_t is_valid_op(server_packet_op_t op);

#define NO_MEM_FATAL "Cannot allocate more memory!"
#define THREAD_CREATE_FATAL "Cannot create new thread!"

/**
 * @brief Reads up to given bytes from given descriptor, saves data to given pre-allocated buffer.
 * @returns read size on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for the routine "read".
*/
int readn(long fd, void* buf, size_t size);

/**
 * @brief Writes buffer up to given size to given descriptor.
 * @returns 1 on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for routine "write".
*/
int writen(long fd, void* buf, size_t size);

// Read a string from a file descriptor of a certain length
// The length read by this function will be the minimum of the length in input and the one sent from the fd
int readn_string(long fd, char* buf, size_t max_len);

// Write a string to a file descriptor 
int writen_string(long fd, const char* buf, size_t len);

#endif