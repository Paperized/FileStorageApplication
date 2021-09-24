#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "client_params.h"

struct client_params {
    bool_t print_help;

    char server_socket_name[MAX_PATHNAME_API_LENGTH];

    int num_file_readed;
    char* dirname_readed_files;
    char* dirname_replaced_files;

    int dirname_file_sendable_num;
    char* dirname_file_sendable;

    linked_list_t* file_list_sendable;
    linked_list_t* file_list_readable;
    linked_list_t* file_list_removable;
    linked_list_t* file_list_lockable;
    linked_list_t* file_list_unlockable;

    int ms_between_requests;
    bool_t print_operations;
};

#define BREAK_ON_NULL(p) if(p == NULL) break

const char const_comma = ',';
#define COMMA &const_comma

client_params_t* g_params = NULL;

void init_client_params(client_params_t** params)
{
    CHECK_FATAL_EQ(*params, malloc(sizeof(client_params_t)), NULL, NO_MEM_FATAL);
    (*params)->print_help = FALSE;

    (*params)->num_file_readed = -1;
    (*params)->dirname_file_sendable_num = -1;

    (*params)->file_list_sendable = ll_create();
    (*params)->file_list_removable = ll_create();
    (*params)->file_list_readable = ll_create();
    (*params)->file_list_lockable = ll_create();
    (*params)->file_list_unlockable = ll_create();

    (*params)->dirname_readed_files = NULL;
    (*params)->dirname_replaced_files = NULL;
    (*params)->dirname_file_sendable = NULL;

    (*params)->ms_between_requests = 0;
    (*params)->print_operations = FALSE;
}

void free_client_params(client_params_t* params)
{
    ll_free(params->file_list_sendable, free);
    ll_free(params->file_list_readable, free);
    ll_free(params->file_list_removable, free);
    ll_free(params->file_list_lockable, free);
    ll_free(params->file_list_unlockable, free);
    free(params->dirname_file_sendable);
    free(params->dirname_readed_files);
    free(params->dirname_replaced_files);
    free(params);
}

void tokenize_filenames_into_ll(char* str, linked_list_t* ll)
{
    if(ll == NULL || str == NULL) return;
    char* save_ptr;

    char* file_name = strtok_r(str, COMMA, &save_ptr);
    if(file_name == NULL) return;
    
    if(ll_add_tail(ll, file_name) != 0)
    {
        printf("Error adding %s to linked list during params read.\n", file_name);
    }

    while((file_name = strtok_r(NULL, COMMA, &save_ptr)) != NULL)
    {
        if(ll_add_tail(ll, file_name) != 0)
        {
            printf("Error adding %s to linked list during params read.\n", file_name);
        }
    }
}

char* client_dirname_replaced_files(client_params_t* params)
{
    RET_IF(!params, NULL);
    return params->dirname_replaced_files;
}

int read_args_client_params(int argc, char** argv, client_params_t* params)
{
    RET_IF(!params, -1);

    char* save_ptr;
    char* n;
    int c;
    int i = 0;
    while ((c = getopt(argc, argv, "hf:w:W:r:R:d:l:u:c:pD:t:")) != -1)
    {
        save_ptr = NULL;
        PRINT_INFO("%d) %s", i++, optarg);

        switch (c)
        {
        case 'h':
            params->print_help = TRUE;
            return 0;
        case 'f':
            BREAK_ON_NULL(optarg);
            if(strnlen(params->server_socket_name, MAX_PATHNAME_API_LENGTH) > 0)
                break;

            strncpy(params->server_socket_name, optarg, MAX_PATHNAME_API_LENGTH);
            break;
        case 'w':
            BREAK_ON_NULL(optarg);
            params->dirname_file_sendable = strtok_r(optarg, COMMA, &save_ptr);
            BREAK_ON_NULL(params->dirname_file_sendable);    
            int dir_num = atoi(strtok_r(NULL, COMMA, &save_ptr));

            params->dirname_file_sendable = realpath(params->dirname_file_sendable, NULL);
            BREAK_ON_NULL(params->dirname_file_sendable);
            params->dirname_file_sendable_num = dir_num;
            break;
        case 'W':
            tokenize_filenames_into_ll(optarg, params->file_list_sendable);
            break;
        case 'r':
            tokenize_filenames_into_ll(optarg, params->file_list_readable);
            break;
        case 'R':
            BREAK_ON_NULL(optarg);
            n = strtok_r(optarg, COMMA, &save_ptr);
            params->num_file_readed = atoi(n);
            break;
        case 'd':
            BREAK_ON_NULL(optarg);
            params->dirname_readed_files = realpath(optarg, NULL);
            break;
        case 'D':
            BREAK_ON_NULL(optarg);
            params->dirname_replaced_files = realpath(optarg, NULL);
            break;
        case 'l':
            tokenize_filenames_into_ll(optarg, params->file_list_lockable);
            break;
        case 'u':
            tokenize_filenames_into_ll(optarg, params->file_list_unlockable);
            break;
        case 't':
            BREAK_ON_NULL(optarg);
            params->ms_between_requests = atoi(optarg);
            break;
        case 'c':
            tokenize_filenames_into_ll(optarg, params->file_list_removable);
            break;
        case 'p':
            params->print_operations = TRUE;
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
    if(!params) return FALSE;

    return params->print_help;
}

char* client_get_socket_name(client_params_t* params)
{
    if(!params) return NULL;

    return params->server_socket_name;
}

int client_num_file_readed(client_params_t* params)
{
    if(!params) return 0;

    return params->num_file_readed;
}

int client_dirname_file_sendable_num(client_params_t* params)
{
    if(!params) return 0;

    return params->dirname_file_sendable_num;
}

char* client_dirname_readed_files(client_params_t* params)
{
    if(!params) return NULL;

    return params->dirname_readed_files;
}

char* client_dirname_file_sendable(client_params_t* params)
{
    if(!params) return NULL;

    return params->dirname_file_sendable;
}

linked_list_t* client_file_list_sendable(client_params_t* params)
{
    if(!params) return NULL;

    return params->file_list_sendable;
}

linked_list_t* client_file_list_readable(client_params_t* params)
{
    if(!params) return NULL;

    return params->file_list_readable;
}

linked_list_t* client_file_list_removable(client_params_t* params)
{
    if(!params) return NULL;

    return params->file_list_removable;
}

linked_list_t* client_file_list_lockable(client_params_t* params)
{
    if(!params) return NULL;

    return params->file_list_lockable;
}

linked_list_t* client_file_list_unlockable(client_params_t* params)
{
    if(!params) return NULL;

    return params->file_list_unlockable;
}

int client_ms_between_requests(client_params_t* params)
{
    if(!params) return 0;

    return params->ms_between_requests;
}

bool_t client_print_operations(client_params_t* params)
{
    if(!params) return FALSE;

    return params->print_operations;
}

void print_params(client_params_t* params)
{
    printf("/********** CLIENT PARAMS **********\\.\n");
    printf("-p Print operation: %s.\n", params->print_operations ? "TRUE" : "FALSE");
    printf("-f Socket file path: %s.\n", params->server_socket_name == NULL ? "NULL" : params->server_socket_name);
    printf("-w Directory sendable: ");
    if(params->dirname_file_sendable)
    {
        printf("%s,%d.\n", params->dirname_file_sendable, params->dirname_file_sendable_num);
    }
    else
    {
        printf("NULL.\n");
    }

    printf("-W Filenames sendable: ");
    if(ll_count(params->file_list_sendable) == 0)
        printf("NONE.\n");
    else
    {
        node_t* curr = ll_get_head_node(params->file_list_sendable);
        while(curr != NULL)
        {
            char* filename = node_get_value(curr);
            printf("%s", filename);
            curr = node_get_next(curr);

            if(curr != NULL)
                printf(", ");
        }

        printf(".\n");
    }

    printf("-r Filenames readable: ");
    if(ll_count(params->file_list_readable) == 0)
        printf("NONE.\n");
    else
    {
        node_t* curr = ll_get_head_node(params->file_list_readable);
        while(curr != NULL)
        {
            char* filename = node_get_value(curr);
            printf("%s", filename);
            curr = node_get_next(curr);

            if(curr != NULL)
                printf(", ");
        }

        printf(".\n");
    }

    printf("-R Number files readable from server: %d.\n", params->num_file_readed);

    printf("-d Dirname files stored (after reading them): %s.\n", params->dirname_readed_files == NULL ? "NULL" : params->dirname_readed_files);
    printf("-D Dirname files stored (after replacement): %s.\n", params->dirname_replaced_files == NULL ? "NULL" : params->dirname_replaced_files);

    printf("-t Time between operation completed (in milliseconds): %d.\n", params->ms_between_requests);
    printf("-l Filenames lockable: ");
    if(ll_count(params->file_list_lockable) == 0)
        printf("NONE.\n");
    else
    {
        node_t* curr = ll_get_head_node(params->file_list_lockable);
        while(curr != NULL)
        {
            char* filename = node_get_value(curr);
            printf("%s", filename);
            curr = node_get_next(curr);

            if(curr != NULL)
                printf(", ");
        }

        printf(".\n");
    }
    printf("-u Filenames unlockable: ");
    if(ll_count(params->file_list_unlockable) == 0)
        printf("NONE.\n");
    else
    {
        node_t* curr = ll_get_head_node(params->file_list_unlockable);
        while(curr != NULL)
        {
            char* filename = node_get_value(curr);
            printf("%s", filename);
            curr = node_get_next(curr);

            if(curr != NULL)
                printf(", ");
        }

        printf(".\n");
    }
    printf("-c Filenames removable: ");
    if(ll_count(params->file_list_removable) == 0)
        printf("NONE.\n");
    else
    {
        node_t* curr = ll_get_head_node(params->file_list_removable);
        while(curr != NULL)
        {
            char* filename = node_get_value(curr);
            printf("%s", filename);
            curr = node_get_next(curr);

            if(curr != NULL)
                printf(", ");
        }

        printf(".\n");
    }

    printf("\\***********************************//.\n");
}