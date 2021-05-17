#ifndef _CLIENT_PARAMS_H_
#define _CLIENT_PARAMS_H_

#include <stdlib.h>
#include "server_api_utils.h"
#include "utils.h"

typedef struct client_params {
    bool_t print_help;

    char server_socket_name[CONFIG_MAX_SOCKET_NAME_LENGTH];

    char* dirname_file_sendable;
    int max_file_sendable;

    char** file_list_sendable;
    int num_file_list_sendable;

    char** file_list_readable;
    int num_file_list_readable;

    char* dirname_readed_files;

    int ms_between_requests;

    char** file_list_removable;
    int num_file_list_removable;

    bool_t print_operations;

} client_params_t;

void init_client_params(client_params_t* params);
int  read_args_client_params(int argc, char** argv, client_params_t* params);
void free_client_params(client_params_t* params);

#endif