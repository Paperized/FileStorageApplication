#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config_params.h"
#include "utils.h"

struct configuration_params {
    unsigned int thread_workers;
    unsigned int bytes_storage_available;
    unsigned int max_files_num;
    char socket_name[MAX_PATHNAME_API_LENGTH];
    char policy_type[MAX_POLICY_LENGTH];
    unsigned int backlog_sockets_num;
};

void print_config_params(const configuration_params_t* config)
{
    if(!config) return;

    printf("********* CONFIGURATION PARAMS *********\n");

    printf("Socket File Name: %s\n", config->socket_name);
    printf("Thread workers: %u\n", config->thread_workers);
    printf("Storage available on startup (in bytes): %u\n", config->bytes_storage_available);
    printf("Max files number: %u\n", config->max_files_num);
    printf("Policy type: %s\n", config->policy_type);
    printf("Backlog sockets count: %u\n", config->backlog_sockets_num);

    printf("****************************************\n");
}

configuration_params_t* load_config_params(const char* config_path_name)
{
    FILE* fptr;
    configuration_params_t* config;

    CHECK_ERROR_EQ(fptr, fopen(config_path_name, "r"), NULL, NULL, "Configuration file cannot be opened!");
    CHECK_FATAL_ERRNO(config, malloc(sizeof(configuration_params_t)), NO_MEM_FATAL);

    fscanf(fptr, "SERVER_SOCKET_NAME=%s\n", config->socket_name);
    fscanf(fptr, "SERVER_THREAD_WORKERS=%u\n", &config->thread_workers);
    fscanf(fptr, "SERVER_BYTE_STORAGE_AVAILABLE=%u\n", &config->bytes_storage_available);
    fscanf(fptr, "SERVER_MAX_FILES_NUM=%u\n", &config->max_files_num);
    fscanf(fptr, "POLICY_NAME=%s\n", config->policy_type);
    fscanf(fptr, "SERVER_BACKLOG_NUM=%u\n", &config->backlog_sockets_num);

    return config;
}

void free_config(configuration_params_t* config)
{
    free(config);
}

unsigned int config_get_num_workers(const configuration_params_t* config)
{
    return config->thread_workers;
}

unsigned int config_get_max_server_size(const configuration_params_t* config)
{
    return config->bytes_storage_available;
}

unsigned int config_get_max_files_count(const configuration_params_t* config)
{
    return config->max_files_num;
}

unsigned int config_get_backlog_sockets_num(const configuration_params_t* config)
{
    return config->backlog_sockets_num;
}

void config_get_socket_name(const configuration_params_t* config, char output[MAX_PATHNAME_API_LENGTH])
{
    strncpy(output, config->socket_name, MAX_PATHNAME_API_LENGTH);
}

void config_get_policy_name(const configuration_params_t* config, char output[MAX_POLICY_LENGTH])
{
    strncpy(output, config->policy_type, MAX_POLICY_LENGTH);
}