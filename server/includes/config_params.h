#ifndef _CONFIG_PARAMS_
#define _CONFIG_PARAMS_

#include "server_api_utils.h"

#define MAX_POLICY_LENGTH 40

typedef struct configuration_params configuration_params_t;

void print_config_params(const configuration_params_t* config);
configuration_params_t* load_config_params(const char* config_path_name);

unsigned int config_get_num_workers(const configuration_params_t* config);
unsigned int config_get_max_server_size(const configuration_params_t* config);
unsigned int config_get_max_files_count(const configuration_params_t* config);
unsigned int config_get_backlog_sockets_num(const configuration_params_t* config);
void config_get_socket_name(const configuration_params_t* config, char output[MAX_PATHNAME_API_LENGTH + 1]);
void config_get_log_name(const configuration_params_t* config, char output[MAX_PATHNAME_API_LENGTH + 1]);
void config_get_policy_name(const configuration_params_t* config, char output[MAX_POLICY_LENGTH + 1]);

void free_config(configuration_params_t* config);


#endif