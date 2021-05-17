#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "client_params.h"

int main(int argc, char **argv)
{
    client_params_t params;
    init_client_params(&params);
    
    int error = read_args_client_params(argc, argv, &params);
    if(error != 0)
    {
        return 0;
    }

    // read flags params

    // connect to server

    // loop client

    free_client_params(&params);
    return 0;
}