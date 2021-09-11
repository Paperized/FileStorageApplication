#include <stdio.h>
#include <stdlib.h>
#include "server.h"

void print_server_error_code(int code)
{
    switch(code)
    {
        case ERR_SOCKET_FAILED:
            printf("Error during the creation of the socket\n");
            break;
        case ERR_SOCKET_BIND_FAILED:
            printf("Error during the binding of the socket\n");
            break;
        case ERR_SOCKET_LISTEN_FAILED:
            printf("Error during the listening of the socket\n");
            break;
        default:
            printf("Status code %d is unknown\n", code);
            break;
    }
}

int main()
{
    configuration_params_t* config = load_config_params("./config.txt");
    if(!config)
    {
        printf("[Error]: Configuration file cannot be opened!\n");
        return -1;
    }

    printf("Server config loaded succesfully!\n");
    print_config_params(loaded_configuration);

    int status = init_server(config);
    if(status != SERVER_OK)
    {
        printf("[Error]: ");
        print_server_error_code(status);
        return -1;
    }

    printf("Server initialized succesfully!\n");

    int exit_status = start_server();
    if(exit_status != SERVER_OK)
    {
        printf("[Error]: ");
        print_server_error_code(status);
        return -1;
    }

    return 0;
}