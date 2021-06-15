#ifndef _CONFIG_PARAMS_
#define _CONFIG_PARAMS_

#include "server_api_utils.h"

#define ERR_READING_CONFIG -1

#define CONFIG_FILE_NAME "config.txt";
#define CONFIG_PATH_FILE "./" CONFIG_FILE_NAME;

typedef struct configuration_params {
    unsigned int thread_workers;
    unsigned int bytes_storage_available;
    char socket_name[MAX_PATHNAME_API_LENGTH];
    char policy_type[40];
    unsigned int backlog_sockets_num;
} configuration_params;

void print_configuration_params(const configuration_params* config);
int load_configuration_params(configuration_params* params);

#endif