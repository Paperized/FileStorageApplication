#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        PRINT_INFO("Usage: server.out ./path_to_config");
        return EXIT_SUCCESS;
    }

    // Load a new config by reading it's pathname from console
    configuration_params_t* config = load_config_params(argv[1]);
    if(config == NULL)
        return EXIT_FAILURE;

    printf("Server config loaded succesfully!\n");
    print_config_params(config);

    // Init the server
    int status = init_server(config);
    if(status != SERVER_OK)
        return EXIT_FAILURE;

    printf("Server initialized succesfully!\n");
    // And run it
    start_server();

    // Then free the config
    free_config(config);
    return EXIT_SUCCESS;
}