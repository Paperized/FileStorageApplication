#ifndef _CONFIG_PARAMS_
#define _CONFIG_PARAMS_

#define ERR_READING_CONFIG -1

#define CONFIG_MAX_SOCKET_NAME_LENGTH 64
#define CONFIG_FILE_NAME "config.txt";
#define CONFIG_PATH_FILE "./" CONFIG_FILE_NAME;

typedef struct configuration_params {
    unsigned thread_workers;
    unsigned int bytes_storage_available;
    char socket_name[CONFIG_MAX_SOCKET_NAME_LENGTH];
    unsigned int max_file_uploadable;
    unsigned int max_file_uploadable_per_client;
    unsigned int backlog_sockets_num;
} configuration_params;

void print_configuration_params(const configuration_params* config);
int load_configuration_params(configuration_params* params);

#endif