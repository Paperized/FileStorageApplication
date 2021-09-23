#ifndef _CLIENT_PARAMS_H_
#define _CLIENT_PARAMS_H_

#include <stdlib.h>
#include "linked_list.h"
#include "server_api_utils.h"
#include "utils.h"

typedef struct string_int_pair string_int_pair_t;
typedef struct client_params client_params_t;

extern client_params_t* g_params;

void init_client_params(client_params_t** params);
int  read_args_client_params(int argc, char** argv, client_params_t* params);
int  check_prerequisited(client_params_t* params);
void free_client_params(client_params_t* params);

bool_t client_is_print_help(client_params_t*);
char* client_get_socket_name(client_params_t*);
int client_num_file_readed(client_params_t*);
char* client_dirname_readed_files(client_params_t*);
char* client_dirname_replaced_files(client_params_t* params);
linked_list_t* client_dirname_file_sendable(client_params_t*);
linked_list_t* client_file_list_sendable(client_params_t*);
linked_list_t* client_file_list_readable(client_params_t*);
linked_list_t* client_file_list_removable(client_params_t*);
int client_ms_between_requests(client_params_t*);
bool_t client_print_operations(client_params_t*);

int pair_get_int(string_int_pair_t*);
char* pair_get_str(string_int_pair_t*);

void print_params(client_params_t* params);

#endif