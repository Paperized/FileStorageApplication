#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "handle_client.h"

#define GET_FILE_AND_ACQUIRE1(hm, hmm, fname, fout)     LOCK_MUTEX(hmm); \
                                                        fout = icl_hash_find(files_stored, fname); \
                                                        if(fout != NULL) \
                                                            LOCK_MUTEX(&fout->rw_mutex);

#define GET_FILE_AND_ACQUIRE2(hm, hmm, fname, fout)     GET_FILE_AND_ACQUIRE1(hm, hmm, fname, fout); \
                                                        if(fout == NULL) \
                                                            UNLOCK_MUTEX(hmm);                                      


#define GET_FILE_AND_ACQUIRE_RG(hm, hmm, fname, fout)   GET_FILE_AND_ACQUIRE1(hm, hmm, fname, fout); \
                                                        UNLOCK_MUTEX(hmm);

// return a quantity of bytes > 0 if exceed the total memory, <= otherwise
int check_memory_capacity(size_t next_alloc_size)
{
    size_t memory_available;
    GET_VAR_MUTEX(current_used_memory, memory_available, &current_used_memory_mutex);
    size_t total_memory;
    GET_VAR_MUTEX(config_get_max_server_size(loaded_configuration), total_memory, &loaded_configuration_mutex);
    return (memory_available + next_alloc_size) - total_memory;
}

void add_memory_usage(size_t to_add)
{
    size_t used_memory;
    LOCK_MUTEX(&current_used_memory_mutex);
    current_used_memory += to_add;
    used_memory = current_used_memory;
    UNLOCK_MUTEX(&current_used_memory_mutex);

    SET_VAR_MUTEX(server_log->max_server_size, used_memory > server_log->max_server_size ?
                                                    used_memory : server_log->max_server_size, &server_log_mutex);
}

void sub_memory_usage(size_t to_add)
{
    LOCK_MUTEX(&current_used_memory_mutex);
    current_used_memory -= to_add;
    UNLOCK_MUTEX(&current_used_memory_mutex);
}

bool_t is_total_memory_enough(size_t amount)
{
    size_t total_memory;
    GET_VAR_MUTEX(config_get_max_server_size(loaded_configuration), total_memory, &loaded_configuration_mutex);

    return total_memory > amount;
}

void find_next_file_by_policy(char** pathname_out, file_stored_t** file_out)
{
    char* curr_path = NULL;
    file_stored_t* curr_file = NULL;

    icl_entry_t* entry = NULL;
    icl_entry_t* top_entry = NULL;

    int x;
    icl_hash_foreach(files_stored, x, entry, curr_path, curr_file) {
        if(top_entry == NULL && entry != NULL && curr_file->can_be_removed && curr_file->size > 0)
        {
            top_entry = entry;
        }
        else
        {
            if(entry != NULL && curr_file->can_be_removed && curr_file->size > 0 && server_policy(top_entry->data, curr_file) > 0)
            {
                top_entry = entry;
            }
        }
    }

    if(top_entry == NULL)
    {
        *pathname_out = NULL;
        *file_out = NULL;
    }
    else
    {
        *pathname_out = top_entry->key;
        *file_out = top_entry->data;
    }
}

void session_remove_file_opened(client_session_t* s, const char* pathname)
{
    node_t* curr = ll_get_head_node(s->files_opened);
    while(curr != NULL)
    {
        if(strcmp(node_get_value(curr), pathname) == 0)
        {
            ll_remove_node(s->files_opened, curr);
            return;
        }

        curr = node_get_next(curr);
    }
}

void on_file_name_removed(void* pathname)
{
    // rimuovo questa da tutti gli utenti
    node_t* curr = ll_get_head_node(clients_connected);
    while(curr != NULL)
    {
        client_session_t* curr_s = node_get_value(curr);
        session_remove_file_opened(curr_s, pathname);

        if(strcmp(pathname, curr_s->prev_file_opened) == 0)
        {
            curr_s->prev_file_opened = NULL;
        }

        curr = node_get_next(curr);
    }

    free(pathname);
}

void on_file_content_removed(void* file)
{
    file_stored_t* fcontent = file;
    free(fcontent->data);
    UNLOCK_MUTEX(&fcontent->rw_mutex);
    pthread_mutex_destroy(&fcontent->rw_mutex);
    free(fcontent);
}

void cache_make_space_fifo(int fd, size_t needed_space)
{
    LOCK_MUTEX(&files_stored_mutex);

    while(needed_space > 0)
    {
        char* pathfile;
        file_stored_t* realfile;

        find_next_file_by_policy(&pathfile, &realfile);
        if(realfile != NULL)
        {
            LOCK_MUTEX(&realfile->rw_mutex);
            icl_hash_delete(files_stored, pathfile, on_file_name_removed, on_file_content_removed);

            sub_memory_usage(realfile->size);
            needed_space -= realfile->size;

            EXEC_WITH_MUTEX(add_logging_entry_str(server_log, CACHE_REPLACEMENT, fd, pathfile), &server_log_mutex);
        }
    }

    UNLOCK_MUTEX(&files_stored_mutex);
}

bool_t session_contains_file_opened(client_session_t* s, const char* pathname)
{
    node_t* curr = ll_get_head_node(s->files_opened);
    while(curr != NULL)
    {
        if(strcmp(node_get_value(curr), pathname) == 0)
            return TRUE;

        curr = node_get_next(curr);
    }

    return FALSE;
}

client_session_t* get_session(int fd)
{
    node_t* curr = ll_get_head_node(clients_connected);
    while(curr != NULL)
    {
        client_session_t* curr_session = node_get_value(curr);
        if(curr_session->fd == fd)
            return curr_session;

        curr = node_get_next(curr);
    }

    return NULL;
}

int handle_open_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_OPEN_FILE request operation.\n", curr);
    int read_result;
    int flags;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &flags, sizeof(int)), -1, -1,
                                 EBADF, "Cannot read flags inside packet! fd(%d)", req->header.fd_sender);
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    printf("Pathname %s.\n", pathname);

    client_session_t* session = get_session(req->header.fd_sender);
    packet_t* response;
    server_errors_t error = ERR_NONE;

    file_stored_t* file;
    GET_FILE_AND_ACQUIRE1(files_stored, &files_stored_mutex, pathname, file);

    if(flags & OP_CREATE)
    {
        // se esiste il il file error
        if(file != NULL)
        {
            error = ERR_FILE_ALREADY_EXISTS;
        }
        else
        {
            EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_ADDED, req->header.fd_sender, pathname), &server_log_mutex);
            file = malloc(sizeof(file_stored_t));
            memset(file, 0, sizeof(file_stored_t));
            clock_gettime(CLOCK_REALTIME, &file->creation_time);
            file->last_use_time = file->creation_time;
            file->can_be_removed = TRUE;

            pthread_mutex_init(&file->rw_mutex, NULL);

            icl_hash_insert(files_stored, pathname, file);
        }
    }
    else
    {
        // se il file non esiste error
        if(file == NULL)
        {
            error = ERR_PATH_NOT_EXISTS;
        }
    }

    if(file != NULL)
        UNLOCK_MUTEX(&file->rw_mutex);
    
    UNLOCK_MUTEX(&files_stored_mutex);

    if(error == ERR_NONE)
    {
        EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_OPENED, req->header.fd_sender, pathname), &server_log_mutex);

        response = create_packet(OP_OK, 0);
        if(session_contains_file_opened(session, pathname) == FALSE)
        {
            ll_add_tail(session->files_opened, pathname);
        }

        int pathlen = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
        session->prev_file_opened = pathname;
        strncpy(session->prev_file_opened, pathname, pathlen);
        session->prev_flags_file = flags;
    }
    else
    {
        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_write_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_WRITE_FILE request operation.\n", curr);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    packet_t* response;
    server_errors_t error = ERR_NONE;
    client_session_t* session = get_session(req->header.fd_sender);

    file_stored_t* curr_file;

    if(session->prev_file_opened != NULL)
        GET_FILE_AND_ACQUIRE_RG(files_stored, &files_stored_mutex, session->prev_file_opened, curr_file);

    if(session->prev_file_opened == NULL)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        void* buffer;
        size_t size_b;
        int res = read_file_util(pathname, &buffer, &size_b);
        bool_t enough_totm = is_total_memory_enough(size_b);
        if(!enough_totm)
        {
            error = ERR_FILE_TOO_BIG;
        }
        else if(res < 0)
        {
            error = ERR_PATH_NOT_EXISTS;
        }
        else
        {
            if(curr_file->data != NULL)
            {
                if(curr_file->size > 0)
                    sub_memory_usage(curr_file->size);

                free(curr_file->data);
            }

            curr_file->data = NULL;
            curr_file->size = 0;

            int exceeded_memory = check_memory_capacity(size_b);
            if(exceeded_memory > 0)
            {
                printf("Server full.\n");
                cache_make_space_fifo(session->fd, exceeded_memory);
            }

            curr_file->data = buffer;
            curr_file->size = size_b;
            add_memory_usage(size_b);

            EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_WROTE, req->header.fd_sender, session->prev_file_opened), &server_log_mutex);
        }

        session->prev_file_opened = NULL;
        UNLOCK_MUTEX(&curr_file->rw_mutex);
    }

    if(error != ERR_NONE)
    {
        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK, 0);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_append_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_APPEND_FILE request operation.\n", curr);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    size_t buff_size = 0;
    void* buffer = NULL;

    packet_t* response;
    server_errors_t error = ERR_NONE;
    client_session_t* session = get_session(req->header.fd_sender);
    session->prev_file_opened = NULL;

    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_FILE_AND_ACQUIRE_RG(files_stored, &files_stored_mutex, pathname, curr_file);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        CHECK_FOR_FATAL(buffer, malloc(sizeof(req->header.len - req->cursor_index)));
        read_data(req, buffer, req->header.len - req->cursor_index);
        bool_t enough_totm = is_total_memory_enough(buff_size);
        if(!enough_totm)
        {
            error = ERR_FILE_TOO_BIG;
        }
        else
        {
            int exceeded_memory = check_memory_capacity(buff_size);
            if(exceeded_memory > 0)
            {
                printf("Server full.\n");
                curr_file->can_be_removed = FALSE;
                cache_make_space_fifo(session->fd, exceeded_memory);
            }

            curr_file->data = realloc(curr_file->data, curr_file->size + buff_size);
            if(curr_file->data == NULL)
            {
                printf("Out of memory.\n");
            }

            memcpy(curr_file->data + curr_file->size, buffer, buff_size);
            add_memory_usage(buff_size);
        }

        UNLOCK_MUTEX(&curr_file->rw_mutex);
    }

    if(error != ERR_NONE)
    {
        EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_APPEND, req->header.fd_sender, pathname), &server_log_mutex);

        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK, 0);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
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
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    packet_t* response;
    server_errors_t error = ERR_NONE;
    client_session_t* session = get_session(req->header.fd_sender);
    session->prev_file_opened = NULL;

    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_FILE_AND_ACQUIRE_RG(files_stored, &files_stored_mutex, pathname, curr_file);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        response = create_packet(OP_OK, curr_file->size);

        if(curr_file->size > 0)
        {
            write_data(response, curr_file->data, curr_file->size);
        }

        UNLOCK_MUTEX(&curr_file->rw_mutex);
    }

    if(error != ERR_NONE)
    {
        EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_APPEND, req->header.fd_sender, pathname), &server_log_mutex);
        
        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_nread_files_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_NREAD_FILES request operation.\n", curr);
    int read_result;
    int n_to_read;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &n_to_read, sizeof(int)), -1, -1,
                                    EBADF, "Cannot read n_to_read inside packet! fd(%d)", req->header.fd_sender);
    bool_t read_all = n_to_read <= 0;
    char dirname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, dirname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read dirname inside packet! fd(%d)", req->header.fd_sender);

    packet_t* response;
    client_session_t* session = get_session(req->header.fd_sender);
    session->prev_file_opened = NULL;

    int i;
    icl_entry_t* curr_entry;
    char* curr_fname;
    file_stored_t* curr_file;
    int total_read = 0;

    icl_hash_foreach_mutex(files_stored, i, curr_entry, curr_fname, curr_file, &files_stored_mutex) {
        if((read_all || n_to_read > 0) && (curr_fname != NULL && curr_file != NULL))
        {
            LOCK_MUTEX(&curr_file->rw_mutex);

            char *fdir, *fn;
            extract_dirname_and_filename(curr_fname, &fdir, &fn);
            char* saving_path = buildpath(dirname, fn, strlen(dirname), strlen(fn));
            printf("Readn: %s.\n", saving_path);
            
            int res = write_file_util(saving_path, curr_file->data, curr_file->size);
            UNLOCK_MUTEX(&curr_file->rw_mutex);

            free(saving_path);

            if(res >= 0)
            {
                n_to_read -= 1;
                total_read += 1;
            }
        }
    }
    icl_hash_foreach_mutex_end(&files_stored_mutex);

    EXEC_WITH_MUTEX(add_logging_entry_int(server_log, FILE_NREAD, session->fd, n_to_read), &server_log_mutex);
    response = create_packet(OP_OK, sizeof(int));
    write_data(response, &total_read, sizeof(int));

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_remove_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_REMOVE_FILE request operation.\n", curr);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    packet_t* response;
    server_errors_t error = ERR_NONE;
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);
    file_stored_t* curr_file;

    if(is_file_opened)
    {
        GET_FILE_AND_ACQUIRE2(files_stored, &files_stored_mutex, pathname, curr_file);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;

        size_t memory_saved = curr_file->size;
        icl_hash_delete(files_stored, pathname, on_file_name_removed, on_file_content_removed);
        sub_memory_usage(memory_saved);
        UNLOCK_MUTEX(&files_stored_mutex);
    }

    if(error != ERR_NONE)
    {
        EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_REMOVED, session->fd, pathname), &server_log_mutex);
        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK, 0);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}

int handle_close_file_req(packet_t* req, pthread_t curr)
{
    printf("[W/%lu] OP_CLOSE_FILE request operation.\n", curr);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", req->header.fd_sender);

    packet_t* response;
    server_errors_t error = ERR_NONE;
    file_stored_t* curr_file;
    client_session_t* session = get_session(req->header.fd_sender);
    bool_t is_file_opened = session_contains_file_opened(session, pathname);

    if(is_file_opened)
    {
        GET_FILE_AND_ACQUIRE_RG(files_stored, &files_stored_mutex, pathname, curr_file);
    }

    // check nella hastable se il file esiste e se l'utente lo ha aperto
    if(!is_file_opened)
        error = ERR_FILE_NOT_OPEN;
    else if(curr_file == NULL)
        error = ERR_PATH_NOT_EXISTS;
    else
    {
        session->prev_file_opened = NULL;
        session_remove_file_opened(session, pathname);
        UNLOCK_MUTEX(&curr_file->rw_mutex);
    }

    if(error != ERR_NONE)
    {
        EXEC_WITH_MUTEX(add_logging_entry_str(server_log, FILE_CLOSED, session->fd, pathname), &server_log_mutex);
        response = create_packet(OP_ERROR, sizeof(server_errors_t));
        write_data(response, &error, sizeof(server_errors_t));
    }
    else
    {
        response = create_packet(OP_OK, 0);
    }

    if(send_packet_to_fd(req->header.fd_sender, response) <= 0)
    {
        printf("fallito.\n");
    }

    destroy_packet(response);
    return 1;
}