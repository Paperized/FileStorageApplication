#include <stdio.h>
#include <string.h>
#include "config_params.h"

void print_configuration_params(const configuration_params* config)
{
    printf("********* CONFIGURATION PARAMS *********\n");

    printf("Socket File Name: %s\n", config->socket_name);
    printf("Thread workers: %u\n", config->thread_workers);
    printf("Storage available on startup (in bytes): %u\n", config->bytes_storage_available);
    printf("Policy type: %s\n", config->policy_type);
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
    fscanf(fptr, "POLICY_NAME=%s\n", params->policy_type);
    fscanf(fptr, "SERVER_BACKLOG_NUM=%u\n", &params->backlog_sockets_num);

    return 0;
}