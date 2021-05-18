#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client_params.h"
#include "file_storage_api.h"

void print_help();

int main(int argc, char **argv)
{
    client_params_t params;
    init_client_params(&params);

    int error = read_args_client_params(argc, argv, &params);
    if(error != 0)
    {
        printf("Missing params or invalid params!.\n");
        return 0;
    }

    if(params.print_help)
    {
        print_help();
        return 0;
    }

    struct timespec t;
    t.tv_sec = 10;
    int connected = openConnection("../../server/bin/socket", 1, t);
    if(connected != 0)
    {
        printf("couldnt connect to the server.\n");
    }
    else
    {
        printf("connected to server.\n");
        sleep(1);

        printf("Sending open file request (%s).\n", params.server_socket_name);
        openFile(params.server_socket_name, OP_CREATE | OP_LOCK);

        sleep(2);
        int closed = closeConnection("../server/bin/socket");
        if(closed != 0)
            printf("error during closeConnection.\n");
        else
            printf("connection closed.\n");
    }

    free_client_params(&params);
    return 0;
}

void print_help()
{
    // hf:w:W:r:R:d:l:u:c:p
    printf("-h print all commands available\n");
    printf("-f pathname, socket file pathname to the server\n");
    printf("-w dirname[,n=0], dirname in which N files will be wrote to the server (n=0 means all)\n");
    printf("-W file1[,file2], which files will be wrote to the server separated by a comma\n");
    printf("-r file1[,file2], which files will be readed to the server separated by a comma\n");
    printf("-R [n=0], read N numbers of files stored inside the server (n=0 means all)\n");
    printf("-d dirname, dirname in which every file reader with -r -R flags will be saved, can only be used with those flags.\n");
    printf("-t time, time between every request-response from the server\n");
    printf("-c file1[,file2], which files will be removed from the server separated by a comma (if they exists)\n");
    printf("-p enable log of each operation\n");
}