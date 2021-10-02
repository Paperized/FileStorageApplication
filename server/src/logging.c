#include "logging.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>

logging_t* create_log()
{
    logging_t* log;
    CHECK_FATAL_EQ(log, malloc(sizeof(struct logging)), NULL, NO_MEM_FATAL);
    memset(log, 0, sizeof(struct logging));

    INIT_MUTEX(&log->write_access_m);
    return log;
}

int start_log(logging_t* log, const char* log_path)
{
    if(!log || !log_path)
    {
        errno = EINVAL;
        return -1;
    }

    int cmp = strncmp(log->log_pathname, log_path, MAX_PATHNAME_API_LENGTH);
    if(cmp == 0)
    {
        if(log->f_ptr)
            return 0;
    }
    else
    {
        if(log->f_ptr)
            fclose(log->f_ptr);

        memcpy(log->log_pathname, log_path, strnlen(log_path, MAX_PATHNAME_API_LENGTH));
    }

    log->f_ptr = fopen(log_path, "a");
    if(!log->f_ptr)
    {
        PRINT_WARNING(errno, "Couldn't open in append logging file %s!", log_path);
        return -1;
    }

    LOG_FORMATTED_LINE(log, "------------------------------ START ------------------------------");
    return 1;
}

int stop_log(logging_t* log)
{
    if(!log)
    {
        errno = EINVAL;
        return -1;
    }

    if(!log->f_ptr)
        return 0;

    LOG_FORMATTED_LINE(log, "------------------------------ END ------------------------------\n");

    fclose(log->f_ptr);
    log->f_ptr = NULL;
    return 1;
}

int __internal_write_log(logging_t* log)
{
    RET_IF(!log, -1);

    int len = strnlen(log->__internal_used_str, MAX_LOG_LINE_LENGTH);
    if(len == 0)
        return 1;

    if(!log->f_ptr)
    {
        return -1;
    }

    int res = fwrite(log->__internal_used_str, len, 1, log->f_ptr);
    // in case of crash we are flushing every log
    fflush(log->f_ptr);

    return res;
}

void free_log(logging_t* log)
{
    NRET_IF(!log);

    pthread_mutex_destroy(&log->write_access_m);
    free(log);
}