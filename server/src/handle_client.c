#include <stdio.h>
#include <string.h>
#include "server.h"
#include "handle_client.h"

client_session_t* get_session(int fd)
{
    node_t* curr = clients_connected.head;
    while(curr != NULL)
    {
        client_session_t* curr_session = curr->value;
        if(curr_session->fd == fd)
            return curr_session;

        curr = curr->next;
    }

    return NULL;
}

void handle_open_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_OPEN_FILE request operation.\n", curr);
    int error_read;
    int* flags = read_data(req, sizeof(int), &error_read);
    if(error_read < 0)
    {
        printf("Error reading flags [%d].\n", error_read);
        return;
    }

    printf("%d.\n", *flags);
    if((*flags) & OP_CREATE)
    {
        printf("Flag OP_CREATE set.\n");
    }

    char* pathname = read_data_str(req, &error_read);
    if(error_read < 0)
    {
        printf("Error reading pathname [%d].\n", error_read);
        free(flags);
        return;
    }

    printf("Pathname %s.\n", pathname);

    client_session_t* session = get_session(req->header.fd_sender);
    packet_t* response;
    server_errors_t error = ERR_NONE;

    if((*flags) & OP_CREATE)
    {
        // se esiste il il file error
        if(1)
            error = ERR_FILE_ALREADY_EXISTS;
    }
    else
    {
        // se il file non esiste error
        if(1)
            error = ERR_PATH_NOT_EXISTS;
    }

    if(error == ERR_NONE)
    {
        response = create_packet(OP_OK);
        session->pathname_open_file = pathname;
        session->flags_open_file = *flags;
    }
    else
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
        free(pathname);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(flags);
}

void handle_write_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_WRITE_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    packet_t* response;
    server_errors_t error = ERR_NONE;

    client_session_t* session = get_session(req->header.fd_sender);
    if(strncmp(pathname, session->pathname_open_file, MAX_PATHNAME_API_LENGTH) != 0)
    {
        error = ERR_FILE_NOT_OPEN;
        printf("Pathname: %s cant be written, please open the file before writing.\n", pathname);
    }

    if(error == ERR_NONE)
    {
        // codice file

        response = create_packet(OP_OK);
        session->pathname_open_file = NULL;
    }
    else
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
        free(pathname);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_append_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_APPEND_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    size_t buff_size;
    void* buffer;

    packet_t* response;
    server_errors_t error = ERR_NONE;

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(1)
    {
        error = ERR_PATH_NOT_EXISTS;
    }
    else if(2)
    {
        error = ERR_FILE_NOT_OPEN;
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        buffer = read_until_end(req, &buff_size, &error_read);
        // do append

        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(buffer);
    free(pathname);
}

void handle_read_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_READ_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    packet_t* response;
    server_errors_t error = ERR_NONE;

    // se l'utente non ha aperto il file o il file non esiste
    if(1)
    {
        error = ERR_FILE_NOT_OPEN;
    } 
    else if(2)
    {
        error = ERR_PATH_NOT_EXISTS;
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        size_t buff_size = 0;
        void* buffer = NULL;

        // leggo il file e setto il buffer

        response = create_packet(OP_OK);
        if(buff_size > 0)
        {
            write_data(response, buffer, buff_size);
            free(buffer);
        }
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_remove_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_REMOVE_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    packet_t* response;
    server_errors_t error = ERR_NONE;

    // se l'utente non ha aperto il file o il file non esiste
    if(1)
    {
        error = ERR_FILE_NOT_OPEN;
    } 
    else if(2)
    {
        error = ERR_PATH_NOT_EXISTS;
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        //rimuovo il file dalla hashtable

        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_close_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_CLOSE_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    packet_t* response;
    server_errors_t error = ERR_NONE;

    // se l'utente non ha aperto il file o il file non esiste
    if(1)
    {
        error = ERR_FILE_NOT_OPEN;
    } 
    else if(2)
    {
        error = ERR_PATH_NOT_EXISTS;
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        // chiudi il file

        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}