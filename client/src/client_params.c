#include <unistd.h>
#include <string.h>

#include "client_params.h"


void init_client_params(client_params_t* params)
{
    params->print_help = FALSE;
    params->dirname_file_sendable = NULL;
    params->max_file_sendable = 0;
    params->file_list_sendable = NULL;
    params->num_file_list_sendable = 0;
    params->file_list_readable = NULL;
    params->num_file_list_readable = 0;
    params->dirname_readed_files = NULL;
    params->ms_between_requests = 0;
    params->file_list_removable = NULL;
    params->num_file_list_removable = 0;
    params->print_operations = FALSE;
}

void free_client_params(client_params_t* params)
{
    if(params->dirname_file_sendable != NULL)
    {
        free(params->dirname_file_sendable);
        params->dirname_file_sendable = NULL;
    }

    if(params->file_list_sendable != NULL)
    {
        for(int i = 0; i < params->num_file_list_sendable; i++)
        {
            free(params->file_list_sendable[i]);
        }

        free(params->file_list_sendable);
        params->file_list_sendable = NULL;
    }

    if(params->file_list_readable != NULL)
    {
        for(int i = 0; i < params->num_file_list_readable; i++)
        {
            free(params->file_list_readable[i]);
        }

        free(params->file_list_readable);
        params->file_list_readable = NULL;
    }

    if(params->file_list_removable != NULL)
    {
        for(int i = 0; i < params->num_file_list_removable; i++)
        {
            free(params->file_list_removable[i]);
        }

        free(params->file_list_removable);
        params->file_list_removable = NULL;
    }

    if(params->dirname_readed_files != NULL)
    {
        free(params->dirname_readed_files);
        params->dirname_readed_files = NULL;
    }
}

int read_args_client_params(int argc, char** argv, client_params_t* params)
{
    int c;
    while ((c = getopt(argc, argv, "hf:w:W:r:R:d:l:u:c:p")) != -1)
    {
        switch (c)
        {
        case 'h':
            params->print_help = TRUE;
            return 0;
        case 'f':
            if(optarg != NULL)
            {
                strncpy(params->server_socket_name, optarg, MAX_PATHNAME_API_LENGTH);
            }
            break;
        case 'w':
            break;
        case 'W':
            break;
        case 'r':
            break;
        case 'R':
            break;
        case 'd':
            break;
        case 'l':
            break;
        case 'u':
            break;
        case 'c':
            break;
        case 'p':
            break;

        default:
            continue; /* ignore */
        }
    }

    return 0;
}