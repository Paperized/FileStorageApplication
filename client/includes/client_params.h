#ifndef _CLIENT_PARAMS_H_
#define _CLIENT_PARAMS_H_

#include <stdlib.h>
#include "queue.h"
#include "server_api_utils.h"
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

typedef struct api_option {
    char op;
    void* args;
} api_option_t;

// PER DOMANI AGGIUNGERE ELEMENTI ALLA CODA DEL CLIENT PARAMS E FARE UN LOOP NELLA CLASSE MAIN

extern client_params_t* g_params;

void init_client_params(client_params_t** params);
int  read_args_client_params(int argc, char** argv, client_params_t* params);
int  check_prerequisited(client_params_t* params);
void free_client_params(client_params_t* params);

#endif