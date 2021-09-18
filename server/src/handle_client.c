#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "handle_client.h"

int handle_open_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_OPEN_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    int flags;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &flags, sizeof(int)), -1, -1,
                                 EBADF, "Cannot read flags inside packet! fd(%d)", sender);
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    PRINT_INFO("Pathname %s.", pathname);

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;

    file_stored_t* file;

    //logic

    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_write_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_WRITE_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;


    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_append_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_APPEND_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    size_t buff_size = 0;
    void* buffer = NULL;

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;

    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    if(buffer)
        free(buffer);
    return 1;
}

int handle_read_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_READ_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;


    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_nread_files_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_NREAD_FILES request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    int n_to_read;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &n_to_read, sizeof(int)), -1, -1,
                                    EBADF, "Cannot read n_to_read inside packet! fd(%d)", sender);
    bool_t read_all = n_to_read <= 0;
    char dirname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, dirname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read dirname inside packet! fd(%d)", sender);

    packet_t* response = NULL;

    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_remove_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_REMOVE_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;

    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_close_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_CLOSE_FILE request operation.\n", curr);
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    packet_t* response = NULL;
    server_errors_t error = ERR_NONE;
 

    if(send_packet_to_fd(sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}