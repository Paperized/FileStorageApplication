#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "client_params.h"

#define BREAK_ON_NULL(p) if(p == NULL) break

#define COMMA ","

client_params_t* g_params = NULL;

void init_client_params(client_params_t** params)
{
    CHECK_FATAL_EQ(*params, malloc(sizeof(client_params_t)), NULL, NO_MEM_FATAL);
    (*params)->print_help = FALSE;
    (*params)->api_operations = create_q();
    (*params)->server_socket_name[0] = '\0';
    (*params)->print_operations = FALSE;
}

static void free_pair_int_str(pair_int_str_t* pair)
{
    NRET_IF(!pair);
    free(pair->str);
    free(pair);
}

static void free_api_option(api_option_t* op)
{
    NRET_IF(!op);
    switch(op->op)
    {
        case 'D':
        case 'd':
            free(op->args);
            break;

        case 'W':
        case 'r':
        case 'c':
        case 'l':
        case 'u':
            free_q(op->args, free);
            break;

        case 'w':
            free_pair_int_str(op->args);
            break;

        default:
            break;
    }

    free(op);
}

void free_client_params(client_params_t* params)
{
    free_q(params->api_operations, FREE_FUNC(free_api_option));
    free(params);
}

static void add_op_filename_list(client_params_t* params, char op, char* str)
{
    NRET_IF(!str || !params);
    char* save_ptr;

    char* file_name = strtok_r(str, COMMA, &save_ptr);
    if(!file_name) return;

    api_option_t* api_opt;
    CHECK_FATAL_EQ(api_opt, malloc(sizeof(api_option_t)), NULL, NO_MEM_FATAL);
    api_opt->op = op;
    queue_t* files_q = create_q();
    api_opt->args = files_q;

    file_name = realpath(file_name, NULL);
    if(!file_name)
    {
        PRINT_ERROR_DEBUG(errno, "Filename is invalid! (realpath)");
    }
    else
        enqueue(files_q, file_name);

    while((file_name = strtok_r(NULL, COMMA, &save_ptr)))
    {
        file_name = realpath(file_name, NULL);

        if(!file_name)
        {
            PRINT_ERROR_DEBUG(errno, "Filename is invalid! (realpath)");
            continue;
        }
        enqueue(files_q, file_name);
    }

    if(count_q(files_q) == 0)
    {
        PRINT_WARNING(ENOENT, "Ignoring option %c because no list is provided!", op);
        free(files_q);
        free(api_opt);
        return;
    }

    enqueue(params->api_operations, api_opt);
}

int read_args_client_params(int argc, char** argv, client_params_t* params)
{
    RET_IF(!params, -1);

    char* save_ptr;
    int c;
    while ((c = getopt(argc, argv, "hf:w:W:r:R:d:l:u:c:pD:t:")) != -1)
    {
        save_ptr = NULL;

        switch (c)
        {
        case 'h':
            params->print_help = TRUE;
            return 0;
        case 'p':
            params->print_operations = TRUE;
            break;
        case 'f':
            BREAK_ON_NULL(optarg);
            if(strnlen(params->server_socket_name, MAX_PATHNAME_API_LENGTH) > 0)
                break;

            strncpy(params->server_socket_name, optarg, MAX_PATHNAME_API_LENGTH);
            size_t end = strnlen(optarg, MAX_PATHNAME_API_LENGTH);
            params->server_socket_name[end] = '\0';
            break;

        case 'w':
            BREAK_ON_NULL(optarg);
            char* dirpath = realpath(strtok_r(optarg, COMMA, &save_ptr), NULL);
            BREAK_ON_NULL(dirpath);
            long num = 0;
            char* n = strtok_r(optarg, COMMA, &save_ptr);
            if(n)
                num = strtol(n, NULL, 10);

            pair_int_str_t* pair;
            CHECK_FATAL_EQ(pair, malloc(sizeof(pair_int_str_t)), NULL, NO_MEM_FATAL);
            pair->str = dirpath;
            pair->num = num;
            
            api_option_t* api_opt;
            CHECK_FATAL_EQ(api_opt, malloc(sizeof(api_option_t)), NULL, NO_MEM_FATAL);
            api_opt->op = c;
            api_opt->args = pair;
            enqueue(params->api_operations, api_opt);
            break;

        case 'W':
        case 'l':
        case 'u':
        case 'c':
        case 'r':
            add_op_filename_list(params, c, optarg);
            break;

        case 'R':
        case 't':
            BREAK_ON_NULL(optarg);
            n = strtok_r(optarg, COMMA, &save_ptr);
            BREAK_ON_NULL(n);
            CHECK_FATAL_EQ(api_opt, malloc(sizeof(api_option_t)), NULL, NO_MEM_FATAL);
            api_opt->op = c;
            api_opt->args = (long*)strtol(n, NULL, 10);
            enqueue(params->api_operations, api_opt);
            break;

        case 'd':
        case 'D':
            BREAK_ON_NULL(optarg);
            dirpath = realpath(strtok_r(optarg, COMMA, &save_ptr), NULL);
            BREAK_ON_NULL(dirpath);

            CHECK_FATAL_EQ(api_opt, malloc(sizeof(api_option_t)), NULL, NO_MEM_FATAL);
            api_opt->op = c;
            api_opt->args = dirpath;
            enqueue(params->api_operations, api_opt);
            break;

        default:
            continue; /* ignore */
        }
    }

    return 0;
}

int check_prerequisited(client_params_t* params)
{
    if(strnlen(params->server_socket_name, MAX_PATHNAME_API_LENGTH) == 0)
    {
        PRINT_FATAL(ENXIO, "Socket name cannot be empty, please use -f flag!");
        return -1;
    }

    return 0;
}

bool_t client_is_print_help(client_params_t* params)
{
    RET_IF(!params, FALSE);

    return params->print_help;
}

char* client_get_socket_name(client_params_t* params)
{
    RET_IF(!params, NULL);

    return params->server_socket_name;
}

bool_t client_print_operations(client_params_t* params)
{
    RET_IF(!params, FALSE);

    return params->print_operations;
}

queue_t* client_get_api_operations(client_params_t* params)
{
    RET_IF(!params, NULL);

    return params->api_operations;
}