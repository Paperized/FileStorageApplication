#ifndef _CLIENT_PARAMS_H_
#define _CLIENT_PARAMS_H_

#include <stdlib.h>
#include "queue.h"
#include "server_api_utils.h"

#undef APP_NAME
#define APP_NAME "Client"
#include "utils.h"

typedef struct client_params {
    bool_t print_help;
    bool_t print_operations;
    char server_socket_name[MAX_PATHNAME_API_LENGTH + 1];
    queue_t* api_operations;
} client_params_t;

typedef struct pair_int_str {
    char* str;
    int num;
} pair_int_str_t;

// Association between an operation and it's argument
typedef struct api_option {
    char op;
    void* args;
} api_option_t;

// Global client params
extern client_params_t* g_params;

// Creats and Initialize a client params
void init_client_params(client_params_t** params);

// Load api commands into this client params
int  read_args_client_params(int argc, char** argv, client_params_t* params);

// Check whether this client params have the required parameters
int  check_prerequisited(client_params_t* params);

// Free this client params
void free_client_params(client_params_t* params);

#endif