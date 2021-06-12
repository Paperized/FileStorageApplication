#include <stdio.h>
#include <string.h>
#include "config_params.h"

void print_configuration_params(const configuration_params* config)
{
    printf("********* CONFIGURATION PARAMS *********\n");

    printf("Socket File Name: %s\n", config->socket_name);
    printf("Thread workers: %u\n", config->thread_workers);
    printf("Storage available on startup (in bytes): %u\n", config->bytes_storage_available);
    printf("Max file number uploadable: %u\n", config->max_file_uploadable);
    printf("Max file number uploadable per client: %u\n", config->max_file_uploadable_per_client);
    printf("Backlog sockets count: %u\n", config->backlog_sockets_num);

    printf("****************************************\n");
}

int load_configuration_params(configuration_params* params)
{
    FILE* fptr;
    
    char* pathName = CONFIG_PATH_FILE;
    if((fptr = fopen(pathName, "r")) == NULL)
    {
        return ERR_READING_CONFIG;
    }

    char socketName[MAX_PATHNAME_API_LENGTH];
    fscanf(fptr, "SERVER_SOCKET_NAME=%s\n", socketName);
    strncpy(params->socket_name, socketName, MAX_PATHNAME_API_LENGTH);

    fscanf(fptr, "SERVER_THREAD_WORKERS=%u\n", &params->thread_workers);
    fscanf(fptr, "SERVER_BYTE_STORAGE_AVAILABLE=%u\n", &params->bytes_storage_available);
    fscanf(fptr, "SERVER_MAX_NUM_UPLOADABLE=%u\n", &params->max_file_uploadable);
    fscanf(fptr, "SERVER_MAX_NUM_UPLOADABLE_CLIENT=%u\n", &params->max_file_uploadable_per_client);
    fscanf(fptr, "SERVER_BACKLOG_NUM=%u\n", &params->backlog_sockets_num);
    
    if(fclose(fptr) == EOF)
    {
        printf("[Warning]: fclose returned value is EOF not 0.\n");
    }

    return 0;
}