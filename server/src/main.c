#include <stdio.h>
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
#include <unistd.h>
void* example_fill_queue(void* param)
{
    while(1)
    {
        pthread_mutex_lock(&clients_queue_mutex);
        enqueue(&clients_queue, 10);
        enqueue(&clients_queue, 10);
        enqueue(&clients_queue, 10);
        enqueue(&clients_queue, 10);

        if(count(&clients_queue) > 4)
        {
            pthread_mutex_unlock(&clients_queue_mutex);
        }

        pthread_cond_signal(&client_received_cond);
        pthread_mutex_unlock(&clients_queue_mutex);
        sleep(1);
    }

    return NULL;
}

int main()
{
    if(load_config_server() == ERR_READING_CONFIG)
    {
        printf("[Error]: Configuration file cannot be opened!\n");
        return -1;
    }

    printf("Server config loaded succesfully!\n");
    print_configuration_params(&loaded_configuration);

    int status = init_server();
    if(status != SERVER_OK)
    {
        printf("[Error]: ");
        print_server_error_code(status);
        return -1;
    }

    printf("Server initialized succesfully!\n");

    pthread_t x;
    pthread_create(&x, NULL, &example_fill_queue, NULL);

    int exit_status = start_server();
    if(exit_status != SERVER_OK)
    {
        printf("[Error]: ");
        print_server_error_code(status);
        return -1;
    }

    return 0;
}