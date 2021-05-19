#ifndef _CLIENT_PARAMS_H_
#define _CLIENT_PARAMS_H_

#include <stdlib.h>
#include "linked_list.h"
#include "server_api_utils.h"
#include "utils.h"

typedef struct string_int_pair
{
    char* str_value;
    int int_value;
} string_int_pair_t;

typedef struct client_params {
    bool_t print_help;

    char server_socket_name[MAX_PATHNAME_API_LENGTH];

    int num_file_readed;
    char* dirname_readed_files;

    linked_list_t dirname_file_sendable;
    linked_list_t file_list_sendable;
    linked_list_t file_list_readable;
    linked_list_t file_list_removable;

    int ms_between_requests;
    bool_t print_operations;

} client_params_t;

void init_client_params(client_params_t* params);
int  read_args_client_params(int argc, char** argv, client_params_t* params);
int  check_prerequisited(client_params_t* params);
void free_client_params(client_params_t* params);

extern client_params_t g_params;

void print_params(client_params_t* params);

#endif