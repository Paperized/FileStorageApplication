#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "handle_client.h"

// return a quantity of bytes > 0 if exceed the total memory, <= otherwise
int check_memory_capacity(size_t next_alloc_size)
{
    size_t memory_available;
    GET_VAR_MUTEX(current_used_memory, memory_available, &current_used_memory_mutex);

    return (memory_available + next_alloc_size) - loaded_configuration.bytes_storage_available;
}

void session_remove_file_opened(client_session_t* s, const char* pathname)
{
    node_t* curr = s->files_opened.head;
    while(curr != NULL)
    {
        if(strcmp(curr->value, pathname) == 0)
        {
            ll_remove_node(&s->files_opened, curr);
            return;
        }

        curr = curr->next;
    }
}

bool_t session_contains_file_opened(client_session_t* s, const char* pathname)
{
    node_t* curr = s->files_opened.head;
    while(curr != NULL)
    {
        if(strcmp(curr->value, pathname) == 0)
            return TRUE;

        curr = curr->next;
    }

    return FALSE;
}

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

void handle_open_file_req(const packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_OPEN_FILE request operation.\n", curr);
    int error_read;
    int* flags = read_data(req, sizeof(int), &error_read);
    if(error_read < 0)
    {
        printf("Error reading flags [%d].\n", error_read);
        return;
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

    file_stored_t* file;
    GET_VAR_MUTEX(icl_hash_find(files_stored, pathname), file, &files_stored_mutex);

    if((*flags) & OP_CREATE)
    {
        // se esiste il il file error
        if(file != NULL)
            error = ERR_FILE_ALREADY_EXISTS;
        else
        {
            file = malloc(sizeof(file_stored_t));
            memset(file, 0, sizeof(file_stored_t));
            pthread_mutex_init(&file->rw_mutex, NULL);
            pthread_mutex_init(&file->counter_mutex, NULL);

            EXEC_WITH_MUTEX(icl_hash_insert(files_stored, pathname, file), &files_stored_mutex);
        }
    }
    else
    {
        // se il file non esiste error
        if(file == NULL)
            error = ERR_PATH_NOT_EXISTS;
    }

    if(error == ERR_NONE)
    {
        response = create_packet(OP_OK);
        if(session_contains_file_opened(session, pathname) == FALSE)
        {
            ll_add_tail(&session->files_opened, pathname);
        }

        int pathlen = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
        session->prev_file_opened = malloc(sizeof(char) * pathlen);
        strncpy(session->prev_file_opened, pathname, pathlen);
        session->prev_flags_file = *flags;
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

void handle_write_file_req(const packet_t* req, pthread_t curr)
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
    file_stored_t* curr_file;
    bool_t can_write = strncmp(pathname, session->prev_file_opened, MAX_PATHNAME_API_LENGTH) == 0;
    
    if(can_write)
    {
        GET_VAR_MUTEX(icl_hash_find(files_stored, session->prev_file_opened), curr_file, &files_stored_mutex);
    }

    if(!can_write)
        error = ERR_FILE_NOT_OPEN;
    if(curr_file == NULL || curr_file->being_removed)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;

        EXEC_WITH_MUTEX(curr_file->req_pending_count += 1, &curr_file->counter_mutex);
        LOCK_MUTEX(&curr_file->rw_mutex);
        if(curr_file->being_removed)
        {
            curr_file->req_pending_count -= 1;

            if(curr_file->req_pending_count == 0)
            {
                UNLOCK_MUTEX(&curr_file->rw_mutex);
                free(curr_file);
            }
            else
                UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
        else
        {
            void* buffer;
            size_t size_b;
            int res = read_file_util(pathname, &buffer, &size_b);

            if(res > 0)
            {
                int exceeded_memory = check_memory_capacity(size_b);
                if(exceeded_memory > 0)
                {
                    // remove files for x bytes
                    printf("Server full.\n");
                }

                curr_file->data = buffer;
                curr_file->size = size_b;
            }
            else
            {
                error = ERR_PATH_NOT_EXISTS;
            }

            UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_append_file_req(const packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_APPEND_FILE request operation.\n", curr);
    int error_read;
    char* pathname = read_data_str(req, &error_read);
    if(pathname == NULL)
    {
        printf("Pathname is empty!.\n");
        return;
    }

    size_t buff_size = 0;
    void* buffer = NULL;

    packet_t* response;
    server_errors_t error = ERR_NONE;
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_VAR_MUTEX(icl_hash_find(files_stored, pathname), curr_file, &files_stored_mutex);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    if(curr_file == NULL || curr_file->being_removed)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;

        EXEC_WITH_MUTEX(curr_file->req_pending_count += 1, &curr_file->counter_mutex);
        LOCK_MUTEX(&curr_file->rw_mutex);
        if(curr_file->being_removed)
        {
            curr_file->req_pending_count -= 1;

            if(curr_file->req_pending_count == 0)
            {
                UNLOCK_MUTEX(&curr_file->rw_mutex);
                free(curr_file);
            }
            else
                UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
        else
        {
            buffer = read_until_end(req, &buff_size, &error_read);
            int exceeded_amount = check_memory_capacity(buff_size);
            if(exceeded_amount > 0)
            {
                // rimuovi qualche file
                printf("Server full.\n");
            }

            curr_file->data = realloc(curr_file->data, curr_file->size + buff_size);
            if(curr_file->data == NULL)
            {
                printf("Out of memory.\n");
            }

            memcpy(curr_file->data + curr_file->size, buffer, buff_size);
            UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    if(buffer)
        free(buffer);
    free(pathname);
}

void handle_read_file_req(const packet_t* req, pthread_t curr)
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
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_VAR_MUTEX(icl_hash_find(files_stored, pathname), curr_file, &files_stored_mutex);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL || curr_file->being_removed)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;
        response = create_packet(OP_OK);
        
        EXEC_WITH_MUTEX(curr_file->req_pending_count += 1, &curr_file->counter_mutex);
        LOCK_MUTEX(&curr_file->rw_mutex);
        if(curr_file->being_removed)
        {
            curr_file->req_pending_count -= 1;

            if(curr_file->req_pending_count == 0)
            {
                UNLOCK_MUTEX(&curr_file->rw_mutex);
                free(curr_file);
            }
            else
                UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
        else
        {
            if(curr_file->size > 0)
            {
                write_data(response, curr_file->data, curr_file->size);
            }

            UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_remove_file_req(const packet_t* req, pthread_t curr)
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
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_VAR_MUTEX(icl_hash_find(files_stored, pathname), curr_file, &files_stored_mutex);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL || curr_file->being_removed)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;

        EXEC_WITH_MUTEX(curr_file->req_pending_count += 1, &curr_file->counter_mutex);
        LOCK_MUTEX(&curr_file->rw_mutex);
        curr_file->req_pending_count -= 1;
        if(curr_file->being_removed)
        {
            if(curr_file->req_pending_count == 0)
            {
                UNLOCK_MUTEX(&curr_file->rw_mutex);
                pthread_mutex_destroy(&curr_file->rw_mutex);
                pthread_mutex_destroy(&curr_file->counter_mutex);
                free(curr_file);
            }
            else
                UNLOCK_MUTEX(&curr_file->rw_mutex);
        }
        else
        {
            curr_file->being_removed = TRUE;
            free(curr_file->data);
            SET_VAR_MUTEX(current_used_memory, current_used_memory + curr_file->size, &current_used_memory_mutex);

            if(curr_file->req_pending_count == 0)
            {
                UNLOCK_MUTEX(&curr_file->rw_mutex);
                pthread_mutex_destroy(&curr_file->rw_mutex);
                pthread_mutex_destroy(&curr_file->counter_mutex);
                free(curr_file);
            }
        }
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}

void handle_close_file_req(const packet_t* req, pthread_t curr)
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
    file_stored_t* curr_file;
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);

    if(is_file_opened)
    {
        GET_VAR_MUTEX(icl_hash_find(files_stored, pathname), curr_file, &files_stored_mutex);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    if(curr_file == NULL || curr_file->being_removed)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;
        session_remove_file_opened(session, pathname);
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR);
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    free(pathname);
}