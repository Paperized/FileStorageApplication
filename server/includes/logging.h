#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "utils.h"
#include "server_api_utils.h"

#define MAX_LOG_LINE_LENGTH 500

typedef struct logging {
    FILE* f_ptr;
    char log_pathname[MAX_PATHNAME_API_LENGTH + 1];
    pthread_mutex_t write_access_m;

    char __internal_used_str[MAX_LOG_LINE_LENGTH + 1];
} logging_t;

logging_t* create_log();
int start_log(logging_t* log, const char* log_path);
int stop_log(logging_t* log);
void free_log(logging_t* log);

int __internal_write_log(logging_t* log, char* str, size_t len);

#define LOG_FORMATTED_LINE(log, message, ...) { \
                                                LOCK_MUTEX(&log->write_access_m); \
                                                snprintf(log->__internal_used_str, MAX_LOG_LINE_LENGTH, message "\n", ## __VA_ARGS__); \
                                                __internal_write_log(log, log->__internal_used_str, strnlen(log->__internal_used_str, MAX_LOG_LINE_LENGTH)); \
                                                UNLOCK_MUTEX(&log->write_access_m); \
                                            }

#define LOG_FORMATTED_N_LINE(log, length, message, ...) { \
                                                char* __n_buffer; \
                                                CHECK_FATAL_EQ(__n_buffer, malloc(length * sizeof(char)), NULL, NO_MEM_FATAL); \
                                                snprintf(__n_buffer, length, message "\n", ## __VA_ARGS__); \
                                                size_t __buff_len = strnlen(__n_buffer, length); \
                                                LOCK_MUTEX(&log->write_access_m); \
                                                __internal_write_log(log, __n_buffer, __buff_len); \
                                                UNLOCK_MUTEX(&log->write_access_m); \
                                                free(__n_buffer); \
                                            }
#endif