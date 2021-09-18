#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main()
{
    configuration_params_t* config = load_config_params("./config.txt");
    if(config == NULL)
        return EXIT_FAILURE;

    printf("Server config loaded succesfully!\n");
    print_config_params(config);

    int status = init_server(config);
    if(status != SERVER_OK)
        return EXIT_FAILURE;

    printf("Server initialized succesfully!\n");
    start_server();

    free_config(config);
    return EXIT_SUCCESS;
}