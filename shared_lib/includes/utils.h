#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef enum bool { FALSE, TRUE } bool_t;

#define START_RED_CONSOLE "\033[31;1m"
#define START_YELLOW_CONSOLE "\e[1;33m"
#define RESET_COLOR_CONSOLE "\033[0m\n"

#define PRINT(level, ...)   PRINT_##level(__VA_ARGS__)

#define PRINT_ERROR(...) printf(START_RED_CONSOLE); \
                            printf("[Error] "); \
                            printf(__VA_ARGS__); \
                            printf(RESET_COLOR_CONSOLE)

#define PRINT_WARNING(...) printf(START_YELLOW_CONSOLE); \
                            printf("[Warning] "); \
                            printf(__VA_ARGS__); \
                            printf(RESET_COLOR_CONSOLE)

#define PRINT_INFO(...)     printf("[Info] "  __VA_ARGS__)

#define EXIT_IF_FATAL(err) if(err == ENOMEM) { \
                                PRINT(ERROR, "Fatal error: %s::%s:%d | %s", __FILE__, __func__, __LINE__, strerror(err)); \
                                exit(err); \
                            }
  
#define CHECK_FOR_FATAL(var, value, err) var = value; \
                                            EXIT_IF_FATAL(err)

#define CHECK_ERROR_EQ(var, value, err, ret_value, ...) if((var = value) == err) { \
                                                            PRINT(ERROR, __VA_ARGS__); \
                                                            PRINT(ERROR, "%s::%s:%d\n", __FILE__, __func__, __LINE__); \
                                                            return ret_value; \
                                                        }

#define CHECK_ERROR_EQ_ERRNO(var, value, err, ret_value, errno_val, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT(ERROR, __VA_ARGS__); \
                                                                            PRINT(ERROR, "%s::%s:%d [%s]\n", __FILE__, __func__, __LINE__, strerror(errno_val)); \
                                                                            return ret_value; \
                                                                        }
                                
#define CHECK_WARNING_EQ_ERRNO(var, value, err, ret_value, errno_val, ...) if((var = value) == err) { \
                                                                            errno = errno_val; \
                                                                            PRINT(WARNING, __VA_ARGS__); \
                                                                            PRINT(WARNING, "%s::%s:%d [%s]\n", __FILE__, __func__, __LINE__, strerror(errno_val)); \
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

#endif