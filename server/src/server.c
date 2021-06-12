#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "server.h"
#include "server_api_utils.h"
#include "handle_client.h"

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    rollback_server_and_quit(); \
                                                                    return status; \
                                                                }

#define CHECK_ERROR(boolean, returned_error) if(boolean) return returned_error

#define BREAK_ON_FAST_QUIT(qv, m)  if((qv = get_quit_signal()) == S_FAST) \
                                    { \
                                        pthread_mutex_unlock(m); \
                                        break; \
                                    }


configuration_params loaded_configuration;
int server_socket_id;

pthread_mutex_t clients_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_set_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t clients_connected_cond = PTHREAD_COND_INITIALIZER;
linked_list_t clients_connected = INIT_EMPTY_LL;
fd_set clients_connected_set;

pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t requests_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
queue_t requests_queue = INIT_EMPTY_QUEUE;

pthread_mutex_t quit_signal_mutex = PTHREAD_MUTEX_INITIALIZER;
quit_signal_t quit_signal = S_NONE;

pthread_mutex_t files_stored_mutex = PTHREAD_MUTEX_INITIALIZER;
icl_hash_t* files_stored = NULL;

/** VARIABLE STATUS **/
bool_t socket_initialized = FALSE;
bool_t workers_initialized = FALSE;
bool_t signals_initialized = FALSE;
bool_t connection_accepter_initialized = FALSE;
bool_t reader_initialized = FALSE;

/** THREAD IDS **/
pthread_t* thread_workers_ids;
pthread_t thread_signals_id;
pthread_t thread_accepter_id;
pthread_t thread_reader_id;

pthread_mutex_t current_used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;
size_t current_used_memory = 0;
unsigned int workers_count = 0;

void free_keys_ht(void* key)
{
    char* pathname = key;
    free(pathname);
}

void free_data_ht(void* key)
{
    file_stored_t* file = key;
    if(file->data)
        free(file->data);

    pthread_mutex_destroy(&file->rw_mutex);
    pthread_mutex_destroy(&file->counter_mutex);

    free(file);
}

int get_max_fid_sessions()
{
    int max = 0;
    node_t* curr = clients_connected.head;
    if(curr == NULL)
        return max;

    while(curr != NULL)
    {
        client_session_t* session = curr->value;
        if(session->fd > max)
            max = session->fd;

        curr = curr->next;
    }

    return max;
}

int load_config_server()
{
    CHECK_ERROR(load_configuration_params(&loaded_configuration) == -1, ERR_READING_CONFIG);
    return SERVER_OK;
}

quit_signal_t get_quit_signal()
{
    quit_signal_t result;
    GET_VAR_MUTEX(quit_signal, result, &quit_signal_mutex);
    return result;
}

void set_quit_signal(quit_signal_t value)
{
    SET_VAR_MUTEX(quit_signal, value, &quit_signal_mutex);
}

void* handle_client_requests(void* data)
{
    pthread_t curr = pthread_self();

    printf("Running worker thread\n");
    quit_signal_t quit_signal;
    while((quit_signal = get_quit_signal()) == S_NONE)
    {
        LOCK_MUTEX(&requests_queue_mutex);
        while(count_q(&requests_queue) == 0)
        {
            if(pthread_cond_wait(&request_received_cond, &requests_queue_mutex) != 0)
            {
                UNLOCK_MUTEX(&requests_queue_mutex);
                continue;
            }

            BREAK_ON_FAST_QUIT(quit_signal, &requests_queue_mutex);
        }

        // quit worker loop if signal
        if(quit_signal == S_FAST)
            break;

        const packet_t* request = cast_to(const packet_t*, dequeue(&requests_queue));
        UNLOCK_MUTEX(&requests_queue_mutex);

        if(request == NULL)
        {
            printf("[W/%lu]: Request null, skipping.\n", curr);
            continue;
        }

        printf("[W/%lu] Handling client with id: %d.\n", curr, request->header.fd_sender);

        // Handle the message
        switch(request->header.op)
        {
            case OP_OPEN_FILE:
                handle_open_file_req(request, curr);
                break;

            case OP_REMOVE_FILE:
                handle_remove_file_req(request, curr);
                break;

            case OP_WRITE_FILE:
                handle_write_file_req(request, curr);
                break;

            case OP_APPEND_FILE:
                handle_append_file_req(request, curr);
                break;
            
            case OP_READ_FILE:
                handle_read_file_req(request, curr);
                break;

            case OP_CLOSE_FILE:
                handle_close_file_req(request, curr);
                break;

            default:
                printf("[W/%lu] Unknown request operation, skipping request.\n", curr);
                break;
        }

        // test sleep

        destroy_packet(request);
        printf("[W/%lu] Finished handling.\n", curr);
    }

    // on close
    printf("Quitting worker.\n");
    return NULL;
}

void* handle_signals(void* params)
{
    sigset_t managed_signals;
    CHECK_ERROR(sigemptyset(&managed_signals) == -1, (void*)ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&managed_signals, SIGQUIT) == -1, (void*)ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&managed_signals, SIGHUP) == -1, (void*)ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&managed_signals, SIGINT) == -1, (void*)ERR_SERVER_SIGNALS);
    CHECK_ERROR(pthread_sigmask(SIG_SETMASK, &managed_signals, NULL) == -1, (void*)ERR_SERVER_SIGNALS);
    
    quit_signal_t local_quit_signal = S_NONE;
    quit_signal_t shared_quit_signal;
    int last_signal;
    while((shared_quit_signal = get_quit_signal()) == S_NONE && local_quit_signal == S_NONE)
    {
        CHECK_ERROR(sigwait(&managed_signals, &last_signal) != 0, (void*)ERR_SERVER_SIGNALS);

        switch(last_signal)
        {
            case SIGQUIT:
            case SIGINT:
                local_quit_signal = S_FAST;
                break;

            case SIGHUP:
                local_quit_signal = S_SOFT;
                break;
        }
    }

    if(shared_quit_signal == S_NONE)
    {
        shared_quit_signal = local_quit_signal;
        SET_VAR_MUTEX(quit_signal, local_quit_signal, &quit_signal_mutex);
    }

    switch(shared_quit_signal)
    {
        case S_NONE:
            printf("\nQuitting with signal: S_NONE\n");
            break;

        case S_SOFT:
            printf("\nQuitting with signal: S_SOFT\n");
            break;

        case S_FAST:
            printf("\nQuitting with signal: S_FAST\n");
            break;

        default:
            printf("\nQuitting with signal: ??\n");
            break;
    }

    return NULL;
}

int initialize_workers()
{
    thread_workers_ids = malloc(loaded_configuration.thread_workers * (sizeof(pthread_t)));
    if(thread_workers_ids == NULL)
        return ERR_SOCKET_OUT_OF_MEMORY;

    for(int i = 0; i < loaded_configuration.thread_workers; ++i)
    {
        CHECK_ERROR(pthread_create(&thread_workers_ids[i], NULL, &handle_client_requests, NULL) != 0, ERR_SOCKET_INIT_WORKERS);
        workers_count += 1;
    }

    workers_initialized = TRUE;
    return SERVER_OK;
}

int enable_needed_signales()
{
    sigset_t bitmask;

    CHECK_ERROR(sigemptyset(&bitmask) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&bitmask, SIGQUIT) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&bitmask, SIGHUP) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(sigaddset(&bitmask, SIGINT) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(pthread_sigmask(SIG_SETMASK, &bitmask, NULL) != 0, ERR_SERVER_SIGNALS);

    return SERVER_OK;
}

int reset_signals()
{
    sigset_t bitmask;

    CHECK_ERROR(sigemptyset(&bitmask) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(pthread_sigmask(SIG_SETMASK, &bitmask, NULL) != 0, ERR_SERVER_SIGNALS);

    return SERVER_OK;
}

int disable_signals()
{
    sigset_t bitmask;

    CHECK_ERROR(sigfillset(&bitmask) == -1, ERR_SERVER_SIGNALS);
    CHECK_ERROR(pthread_sigmask(SIG_SETMASK, &bitmask, NULL) != 0, ERR_SERVER_SIGNALS);

    return SERVER_OK;
}

int initialize_signals()
{
    CHECK_ERROR(pthread_create(&thread_signals_id, NULL, &handle_signals, NULL), ERR_SERVER_SIGNALS);
    signals_initialized = TRUE;

    // Reset signals masked in the initialization
    return SERVER_OK;
}

void* handle_connections(void* params)
{
    while(TRUE)
    {
        bool_t add_failed = FALSE;

        printf("Waiting for new connections.\n");
        int new_id = accept(server_socket_id, NULL, 0);
        if(new_id == -1)
            continue;

        // add to queue

        client_session_t* new_client = malloc(sizeof(client_session_t));
        memset(new_client, 0, sizeof(new_client));
        new_client->fd = new_id;
        
        LOCK_MUTEX(&clients_list_mutex);
        add_failed = ll_add_head(&clients_connected, new_client) != 0;
        UNLOCK_MUTEX(&clients_list_mutex);

        // if something go wrong with the queue we close the connection right away
        if(add_failed)
        {
            free(new_client);
            printf("Error: some problems may occourred with malloc in ll_add_head.\n");
            close(new_id);

            LOCK_MUTEX(&clients_set_mutex);
            if(FD_ISSET(new_id, &clients_connected_set))
            {
                FD_CLR(new_id, &clients_connected_set);
            }
            UNLOCK_MUTEX(&clients_set_mutex);
        }
        else
        {
            LOCK_MUTEX(&clients_set_mutex);
            if(!FD_ISSET(new_id, &clients_connected_set))
            {
                FD_SET(new_id, &clients_connected_set);
            }
            UNLOCK_MUTEX(&clients_set_mutex);

            pthread_cond_signal(&clients_connected_cond);
        }
    }

    return NULL;
}

int initialize_connection_accepter()
{
    FD_ZERO(&clients_connected_set);
    CHECK_ERROR(pthread_create(&thread_accepter_id, NULL, &handle_connections, NULL) != 0, ERR_SOCKET_INIT_ACCEPTER);

    connection_accepter_initialized = TRUE;
    return SERVER_OK;
}

void* handle_clients_packets()
{
    quit_signal_t quit_signal = S_NONE;
    while((quit_signal= get_quit_signal()) != S_FAST)
    {
        struct timeval tv;

        // wait 0.1s per check
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        LOCK_MUTEX(&clients_list_mutex);
        size_t n_clients;
        while((n_clients = ll_count(&clients_connected)) == 0)
        {
            pthread_cond_wait(&clients_connected_cond, &clients_list_mutex);

            BREAK_ON_FAST_QUIT(quit_signal, &clients_list_mutex);
        }

        if(quit_signal == S_FAST)
            break;

        int max_fd_clients = get_max_fid_sessions();
        UNLOCK_MUTEX(&clients_list_mutex);

        fd_set current_clients;
        GET_VAR_MUTEX(clients_connected_set, current_clients, &clients_set_mutex);

        int res = select(max_fd_clients + 1, &current_clients, NULL, NULL, &tv);
        if(res <= 0)
            continue;

        LOCK_MUTEX(&clients_list_mutex);
        node_t* curr_client = ll_get_head_node(&clients_connected);

        int error;
        while(curr_client != NULL)
        {
            client_session_t* curr_session = curr_client->value;

            if(FD_ISSET(curr_session->fd, &current_clients))
            {
                printf("reading packet %d.\n", curr_session->fd);
                packet_t* req = read_packet_from_fd(curr_session->fd, &error);

                if(error == 0)
                {
                    if(req->header.op == OP_CLOSE_CONN)
                    {
                        // chiudo la connessione
                        LOCK_MUTEX(&clients_set_mutex);
                        FD_CLR(curr_session->fd, &clients_connected_set);
                        UNLOCK_MUTEX(&clients_set_mutex);

                        node_t* temp = curr_client->next;
                        ll_remove_node(&clients_connected, curr_client);
                        curr_client = temp;

                        if(curr_session->prev_file_opened != NULL)
                            free(curr_session->prev_file_opened);
                        
                        while(ll_count(&curr_session->files_opened) > 0)
                        {
                            void* file;
                            ll_remove_last(&curr_session->files_opened, &file);
                        }

                        free(curr_session);
                        free(req);
                        continue;
                    }

                    LOCK_MUTEX(&requests_queue_mutex);

                    enqueue_m(&requests_queue, req);
                    if(count_q(&requests_queue) == 1)
                        pthread_cond_signal(&request_received_cond);

                    UNLOCK_MUTEX(&requests_queue_mutex);
                }

                // we ignore failed packets returned by the read
            }

            curr_client = curr_client->next;
        }
        UNLOCK_MUTEX(&clients_list_mutex);
    }

    printf("Quitting packet reader.\n");
    return NULL;
}

int initialize_reader()
{
    CHECK_ERROR(pthread_create(&thread_reader_id, NULL, &handle_clients_packets, NULL) != 0, ERR_SERVER_INIT_READER);

    reader_initialized = TRUE;
    return SERVER_OK;
}

void rollback_server_and_quit()
{
    printf("Rollback server and quitting.\n");

    // wait all threads to finish
    if(signals_initialized)
    {
        printf("Quitting signal thread.\n");
        pthread_cancel(thread_signals_id);
        pthread_join(thread_signals_id, NULL);
    }

    // Capire come chiudere i workers in attesa di una condizione
    if(workers_count > 0)
    {
        printf("Quitting workers thread.\n");
        for(int i = 0; i < workers_count; ++i)
        {
            pthread_cancel(thread_workers_ids[i]);
            pthread_join(thread_workers_ids[i], NULL);
        }
    }

    icl_hash_destroy(files_stored, free_keys_ht, free_data_ht);

    if(reader_initialized)
    {
        printf("Quitting reader thread.\n");
        pthread_cancel(thread_reader_id);
        pthread_join(thread_reader_id, NULL);
    }

    printf("Freeing memory.\n");
    free(thread_workers_ids);

    if(socket_initialized)
    {
        printf("Closing socket and removing it.\n");
        close(server_socket_id);
        remove(loaded_configuration.socket_name);
    }

    printf("Resetting signals.\n");
    reset_signals();
}

int wait_server_end()
{
    quit_signal_t closing_signal;

    // wait for a closing signal
    pthread_join(thread_signals_id, NULL);
    GET_VAR_MUTEX(quit_signal, closing_signal, &quit_signal_mutex);

    pthread_cancel(thread_accepter_id);
    pthread_join(thread_accepter_id, NULL);

    if(closing_signal == S_FAST)
        pthread_cond_broadcast(&request_received_cond);

    printf("Joining workers thread.\n");
    for(int i = 0; i < loaded_configuration.thread_workers; ++i)
    {
        //if(closing_signal == S_FAST)
        //    pthread_cancel(thread_workers_ids[i]);

        pthread_join(thread_workers_ids[i], NULL);
    }

    icl_hash_destroy(files_stored, free_keys_ht, free_data_ht);

    // The thread might be locked in a wait
    if(closing_signal == S_FAST)
    {
        printf("Joining reader thread.\n");
        pthread_cond_signal(&clients_connected_cond);
        pthread_join(thread_reader_id, NULL);
    }

    printf("Freeing memory.\n");
    free(thread_workers_ids);

    LOCK_MUTEX(&clients_list_mutex);
    while(ll_count(&clients_connected) > 0)
    {
        printf("count: %lu.\n", ll_count(&clients_connected));
        void* client_id = NULL;
        ll_remove_first(&clients_connected, client_id);
    }
    UNLOCK_MUTEX(&clients_list_mutex);

    printf("Closing socket and removing it.\n");
    close(server_socket_id);
    remove(loaded_configuration.socket_name);

    printf("Resetting signals.\n");
    reset_signals();

    // this function returns only SERVER_OK but can be expanded with other return values
    return SERVER_OK;
}

int start_server()
{
    int lastest_status;

    files_stored = icl_hash_create(10, NULL, NULL);

    // Initialize and run signals
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_signals, lastest_status);
    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_accepter, lastest_status);
    // Initialize and run reader
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_reader, lastest_status);

    printf("Server started with PID:%d.\n", getpid());

    // wait until end
    return wait_server_end();
}

int init_server()
{
    CHECK_ERROR(disable_signals() == ERR_SERVER_SIGNALS, ERR_SERVER_SIGNALS);

    server_socket_id = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK_ERROR(server_socket_id == -1, ERR_SOCKET_FAILED);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, loaded_configuration.socket_name, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    remove(loaded_configuration.socket_name);

    CHECK_ERROR(bind(server_socket_id, (struct sockaddr*)&sa, sizeof(sa)) == -1, ERR_SOCKET_BIND_FAILED);
    CHECK_ERROR(listen(server_socket_id, loaded_configuration.backlog_sockets_num) == -1, ERR_SOCKET_LISTEN_FAILED);

    socket_initialized = TRUE;
    return SERVER_OK;
}