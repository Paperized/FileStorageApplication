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

typedef struct server {
    int server_socket_id;

    configuration_params_t* config;
    pthread_mutex_t config_mutex;

    quit_signal_t quit_signal;
    pthread_mutex_t quit_signal_mutex;

    pthread_cond_t clients_connected_cond;
    linked_list_t* clients_connected;
    pthread_mutex_t clients_list_mutex;

    fd_set clients_connected_set;
    pthread_mutex_t clients_set_mutex;

    pthread_cond_t request_received_cond;
    pthread_mutex_t requests_queue_mutex;
    queue_t* requests_queue;

    file_system_t* fs;
} server_t;

server_t* singleton_server = NULL;

pthread_mutex_t server_log_mutex = PTHREAD_MUTEX_INITIALIZER;
logging_t* server_log;

/** VARIABLE STATUS **/
bool_t socket_initialized = FALSE;
bool_t workers_initialized = FALSE;
bool_t signals_initialized = FALSE;
bool_t connection_accepter_initialized = FALSE;
bool_t reader_initialized = FALSE;

/** THREAD IDS **/
pthread_t* thread_workers_ids;
pthread_t thread_accepter_id;
pthread_t thread_reader_id;

unsigned int workers_count = 0;

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    SET_VAR_MUTEX(singleton_server->quit_signal, S_FAST, &singleton_server->quit_signal_mutex); \
                                                                    server_wait_for_threads(); \
                                                                    server_cleanup(); \
                                                                    return status; \
                                                                }

#define BREAK_ON_FAST_QUIT(qv, m)  if((qv = get_quit_signal()) == S_FAST) \
                                    { \
                                        UNLOCK_MUTEX(m); \
                                        break; \
                                    }

quit_signal_t get_quit_signal()
{
    quit_signal_t result;
    GET_VAR_MUTEX(singleton_server->quit_signal, result, &singleton_server->quit_signal_mutex);
    return result;
}

void set_quit_signal(quit_signal_t value)
{
    SET_VAR_MUTEX(singleton_server->quit_signal, value, &singleton_server->quit_signal_mutex);
}

file_system_t* get_fs()
{
    return singleton_server->fs;
}

void* handle_client_requests(void* data)
{
    pthread_t curr = pthread_self();

    PRINT_INFO("Running worker thread %lu.", curr);
    quit_signal_t quit_signal;
    while((quit_signal = get_quit_signal()) != S_FAST)
    {
        LOCK_MUTEX(&singleton_server->requests_queue_mutex);
        while(count_q(singleton_server->requests_queue) == 0)
        {
            if(pthread_cond_wait(&singleton_server->request_received_cond, &singleton_server->requests_queue_mutex) != 0)
            {
                UNLOCK_MUTEX(&singleton_server->requests_queue_mutex);
                continue;
            }

            BREAK_ON_FAST_QUIT(quit_signal, &singleton_server->requests_queue_mutex);
        }

        // quit worker loop if signal
        if(quit_signal == S_FAST)
            break;

        packet_t* request = (packet_t*)dequeue(singleton_server->requests_queue);
        UNLOCK_MUTEX(&singleton_server->requests_queue_mutex);

        if(request == NULL)
        {
            PRINT_WARNING(EINVAL, "[W/%lu] Request null, skipping.", curr);
            continue;
        }

        PRINT_INFO("[W/%lu] Handling client with id %d.", curr, packet_get_sender(request));
        packet_t* res_packet = create_packet(OP_OK, 0);

        int res;
        // Handle the message
        switch(packet_get_op(request))
        {
            case OP_OPEN_FILE:
                PRINT_INFO("[W/%lu] OP_OPEN_FILE request operation.", curr);
                res = handle_open_file_req(request, res_packet);
                break;

            case OP_LOCK_FILE:
                PRINT_INFO("[W/%lu] OP_LOCK_FILE request operation.", curr);
                res = handle_lock_file_req(request, res_packet);
                break;

            case OP_UNLOCK_FILE:
                PRINT_INFO("[W/%lu] OP_UNLOCK_FILE request operation.", curr);
                res = handle_unlock_file_req(request, res_packet);
                break;

            case OP_REMOVE_FILE:
                PRINT_INFO("[W/%lu] OP_REMOVE_FILE request operation.", curr);
                res = handle_remove_file_req(request, res_packet);
                break;

            case OP_WRITE_FILE:
                PRINT_INFO("[W/%lu] OP_WRITE_FILE request operation.", curr);
                res = handle_write_file_req(request, res_packet);
                break;

            case OP_APPEND_FILE:
                PRINT_INFO("[W/%lu] OP_APPEND_FILE request operation.", curr);
                res = handle_append_file_req(request, res_packet);
                break;
            
            case OP_READ_FILE:
                PRINT_INFO("[W/%lu] OP_READ_FILE request operation.", curr);
                res = handle_read_file_req(request, res_packet);
                break;

            case OP_READN_FILES:
                PRINT_INFO("[W/%lu] OP_READN_FILES request operation.", curr);
                res = handle_nread_files_req(request, res_packet);
                break;

            case OP_CLOSE_FILE:
                PRINT_INFO("[W/%lu] OP_CLOSE_FILE request operation.", curr);
                res = handle_close_file_req(request, res_packet);
                break;

            default:
                PRINT_INFO("[W/%lu] Unknown request operation, skipping request.", curr);
                break;
        }

        // res == -1 doesn't send any packet, a future request will take care of this (e.g. locks)
        if(res > -1)
        {
            // if its an errno value
            if(res != 0)
            {
                packet_set_op(res_packet, OP_ERROR);
                write_data(res_packet, &res, sizeof(int));
            }

            // send the packet OK/ERROR
            res = send_packet_to_fd(packet_get_sender(request), res_packet);
            if(res == -1)
            {
                PRINT_WARNING(errno, "Cannot send error packet to fd(%d)!", packet_get_sender(request));
            }
        }

        destroy_packet(request);
        destroy_packet(res_packet);
        PRINT_INFO("[W/%lu] Finished handling.", curr);
    }

    // on close
    PRINT_INFO("Quitting worker.");
    return NULL;
}

quit_signal_t server_wait_end_signal()
{
    sigset_t managed_signals;
    int error;
    CHECK_ERROR_EQ(error, sigemptyset(&managed_signals), -1, S_FAST, "Coudln't clear signals!");
    CHECK_ERROR_EQ(error, sigaddset(&managed_signals, SIGQUIT), -1, S_FAST, "Couldn't add SIGQUIT to sigset!");
    CHECK_ERROR_EQ(error, sigaddset(&managed_signals, SIGHUP), -1, S_FAST, "Couldn't add SIGHUP to sigset!");
    CHECK_ERROR_EQ(error, sigaddset(&managed_signals, SIGINT), -1, S_FAST, "Couldn't add SIGINT to sigset!");
    CHECK_ERROR_EQ(error, pthread_sigmask(SIG_SETMASK, &managed_signals, NULL), -1, S_FAST, "Couldn't set new sigmask!");
    
    // ignore sigpipe

    quit_signal_t local_quit_signal = S_NONE;
    quit_signal_t shared_quit_signal;
    int last_signal;
    signals_initialized = TRUE;

    while((shared_quit_signal = get_quit_signal()) == S_NONE && local_quit_signal == S_NONE)
    {
        CHECK_ERROR_NEQ(error, sigwait(&managed_signals, &last_signal), 0, S_FAST, "Sigwait failed!");

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
        SET_VAR_MUTEX(singleton_server->quit_signal, local_quit_signal, &singleton_server->quit_signal_mutex);
    }

    // debug
    switch(shared_quit_signal)
    {
        case S_NONE:
            PRINT_INFO("\nQuitting with signal: S_NONE.");
            break;

        case S_SOFT:
            PRINT_INFO("\nQuitting with signal: S_SOFT.");
            break;

        case S_FAST:
            PRINT_INFO("\nQuitting with signal: S_FAST.");
            break;

        default:
            PRINT_INFO("\nQuitting with signal: ??.");
            break;
    }

    return shared_quit_signal;
}

int initialize_workers()
{
    CHECK_FATAL_EQ(thread_workers_ids, malloc(config_get_num_workers(singleton_server->config) * (sizeof(pthread_t))), NULL, NO_MEM_FATAL);

    int error;
    for(int i = 0; i < config_get_num_workers(singleton_server->config); ++i)
    {
        CHECK_ERROR_NEQ(error, pthread_create(&thread_workers_ids[i], NULL, &handle_client_requests, NULL), 0,
                 ERR_SOCKET_INIT_WORKERS, "Coudln't create the %dth thread!", i);
        workers_count += 1;
    }

    workers_initialized = TRUE;
    return SERVER_OK;
}

void* handle_connections(void* params)
{
    while(TRUE)
    {
        bool_t add_failed = FALSE;

        PRINT_INFO("Waiting for new connections.");
        int new_id = accept(singleton_server->server_socket_id, NULL, 0);
        if(new_id == -1)
            continue;
        
        int* new_client;
        MAKE_COPY(new_client, int, new_id);
        SET_VAR_MUTEX(add_failed, ll_add_head(singleton_server->clients_connected, new_client) != 0, &singleton_server->clients_list_mutex);

        // if something go wrong with the queue we close the connection right away
        if(add_failed)
        {
            PRINT_WARNING(errno, "ll_add_head failed!.");
            free(new_client);
            close(new_id);

            LOCK_MUTEX(&singleton_server->clients_set_mutex);
            if(FD_ISSET(new_id, &singleton_server->clients_connected_set))
            {
                FD_CLR(new_id, &singleton_server->clients_connected_set);
            }
            UNLOCK_MUTEX(&singleton_server->clients_set_mutex);
        }
        else
        {
            LOCK_MUTEX(&singleton_server->clients_set_mutex);
            if(!FD_ISSET(new_id, &singleton_server->clients_connected_set))
            {
                FD_SET(new_id, &singleton_server->clients_connected_set);
            }
            UNLOCK_MUTEX(&singleton_server->clients_set_mutex);

            EXEC_WITH_MUTEX(add_logging_entry(server_log, CLIENT_JOINED, new_id, NULL), &server_log_mutex);

            COND_SIGNAL(&singleton_server->clients_connected_cond);
        }
    }

    return NULL;
}

int initialize_connection_accepter()
{
    int error;
    FD_ZERO(&singleton_server->clients_connected_set);
    CHECK_ERROR_NEQ(error, pthread_create(&thread_accepter_id, NULL, &handle_connections, NULL), 0, ERR_SOCKET_INIT_ACCEPTER, THREAD_CREATE_FATAL);

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

        LOCK_MUTEX(&singleton_server->clients_list_mutex);
        size_t n_clients;
        while((n_clients = ll_count(singleton_server->clients_connected)) == 0)
        {
            COND_WAIT(&singleton_server->clients_connected_cond, &singleton_server->clients_list_mutex);
            BREAK_ON_FAST_QUIT(quit_signal, &singleton_server->clients_list_mutex);
        }

        if(quit_signal == S_FAST)
            break;

        int max_fd_clients = ll_get_max_int(singleton_server->clients_connected);
        UNLOCK_MUTEX(&singleton_server->clients_list_mutex);

        fd_set current_clients;
        GET_VAR_MUTEX(singleton_server->clients_connected_set, current_clients, &singleton_server->clients_set_mutex);

        int res = select(max_fd_clients + 1, &current_clients, NULL, NULL, &tv);
        if(res <= 0)
            continue;

        LOCK_MUTEX(&singleton_server->clients_list_mutex);
        node_t* curr_client = ll_get_head_node(singleton_server->clients_connected);

        while(curr_client != NULL)
        {
            int curr_session = *((int*)node_get_value(curr_client));

            if(FD_ISSET(curr_session, &current_clients))
            {
                PRINT_INFO("reading packet %d.", curr_session);
                packet_t* req = read_packet_from_fd(curr_session);

                if(is_packet_valid(req))
                {
                    if(packet_get_op(req) == OP_CLOSE_CONN)
                    {
                        // chiudo la connessione
                        EXEC_WITH_MUTEX(FD_CLR(curr_session, &singleton_server->clients_connected_set), &singleton_server->clients_set_mutex);

                        node_t* temp = node_get_next(curr_client);
                        ll_remove_node(singleton_server->clients_connected, curr_client);
                        curr_client = temp;
                        
                        EXEC_WITH_MUTEX(add_logging_entry(server_log, CLIENT_LEFT, curr_session, NULL), &server_log_mutex);

                        free(req);
                        continue;
                    }

                    LOCK_MUTEX(&singleton_server->requests_queue_mutex);

                    enqueue_m(singleton_server->requests_queue, req);
                    if(count_q(singleton_server->requests_queue) == 1)
                    {
                        COND_SIGNAL(&singleton_server->request_received_cond);
                    }

                    UNLOCK_MUTEX(&singleton_server->requests_queue_mutex);
                }

                // we ignore failed packets returned by the read
            }

            curr_client = node_get_next(curr_client);
        }
        UNLOCK_MUTEX(&singleton_server->clients_list_mutex);
    }

    PRINT_INFO("Quitting packet reader.");
    return NULL;
}

int initialize_reader()
{
    int error;
    CHECK_ERROR_NEQ(error, pthread_create(&thread_reader_id, NULL, &handle_clients_packets, NULL), 0, ERR_SERVER_INIT_READER, THREAD_CREATE_FATAL);

    reader_initialized = TRUE;
    return SERVER_OK;
}

int server_wait_for_threads()
{
    quit_signal_t closing_signal;

    // wait for a closing signal
    GET_VAR_MUTEX(singleton_server->quit_signal, closing_signal, &singleton_server->quit_signal_mutex);

    if(connection_accepter_initialized)
    {
        pthread_cancel(thread_accepter_id);
        pthread_join(thread_accepter_id, NULL);
    }

    if(closing_signal == S_FAST)
    {
        COND_BROADCAST(&singleton_server->request_received_cond);
    }

    PRINT_INFO("Joining workers thread.");
    for(int i = 0; i < workers_count; ++i)
    {
        //if(closing_signal == S_FAST)
        //    pthread_cancel(thread_workers_ids[i]);

        pthread_join(thread_workers_ids[i], NULL);
    }

    // The thread might be locked in a wait
    if(closing_signal == S_FAST)
    {
        PRINT_INFO("Joining reader thread.");
        COND_SIGNAL(&singleton_server->clients_connected_cond);
        pthread_join(thread_reader_id, NULL);
    }

    return SERVER_OK;
}

int server_cleanup()
{
    PRINT_INFO("Freeing memory.");

    free_fs(singleton_server->fs);
    ll_free(singleton_server->clients_connected, free);
    free_q(singleton_server->requests_queue, (void(*)(void*))destroy_packet);
    free(thread_workers_ids);
    free(p_on_file_deleted_locks);

    PRINT_INFO("Closing socket and removing it.");
    close(singleton_server->server_socket_id);

    pthread_mutex_destroy(&singleton_server->config_mutex);
    pthread_mutex_destroy(&singleton_server->quit_signal_mutex);
    pthread_mutex_destroy(&singleton_server->clients_list_mutex);
    pthread_mutex_destroy(&singleton_server->clients_set_mutex);
    pthread_mutex_destroy(&singleton_server->requests_queue_mutex);

    pthread_cond_destroy(&singleton_server->clients_connected_cond);
    pthread_cond_destroy(&singleton_server->request_received_cond);

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(singleton_server->config, socket_name);
    remove(socket_name);

    print_logging(server_log);
    free_log(server_log);

    // this function returns only SERVER_OK but can be expanded with other return values
    return SERVER_OK;
}

int start_server()
{
    int lastest_status;

    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_accepter, lastest_status);
    // Initialize and run reader
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_reader, lastest_status);

    PRINT_INFO("Server started with PID:%d.", getpid());

    // blocking call, return on quit signal
    server_wait_end_signal();
    server_wait_for_threads();

    return server_cleanup();
}

int init_server(const configuration_params_t* config)
{
    INIT_MUTEX(&singleton_server->config_mutex);
    INIT_MUTEX(&singleton_server->quit_signal_mutex);
    INIT_MUTEX(&singleton_server->clients_list_mutex);
    INIT_MUTEX(&singleton_server->clients_set_mutex);
    INIT_MUTEX(&singleton_server->requests_queue_mutex);

    INIT_COND(&singleton_server->clients_connected_cond);
    INIT_COND(&singleton_server->request_received_cond);

    singleton_server->clients_connected = ll_create();
    singleton_server->requests_queue = create_q();

    singleton_server->fs = create_fs(config_get_max_server_size(config),
                                     config_get_max_files_count(config));
    server_log = create_log();

    singleton_server->config = (configuration_params_t*)config;
    // unique packet used many times
    p_on_file_deleted_locks = create_packet(OP_ERROR, sizeof(int));
    int err_packet = EIDRM;
    write_data(p_on_file_deleted_locks, &err_packet, sizeof(int));

    CHECK_ERROR_EQ(singleton_server->server_socket_id, socket(AF_UNIX, SOCK_STREAM, 0), -1, ERR_SOCKET_FAILED, "Couldn't initialize socket!");

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(singleton_server->config, socket_name);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, socket_name, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    remove(socket_name);

    int error;
    CHECK_ERROR_EQ(error,
                    bind(singleton_server->server_socket_id, (struct sockaddr*)&sa, sizeof(sa)), -1,
                    ERR_SOCKET_BIND_FAILED, "Couldn't bind socket!");
    CHECK_ERROR_EQ(error,
                    listen(singleton_server->server_socket_id, config_get_backlog_sockets_num(singleton_server->config)), -1,
                    ERR_SOCKET_LISTEN_FAILED, "Couldn't listen socket!");

    socket_initialized = TRUE;
    return SERVER_OK;
}

void print_logging(logging_t* lg)
{
    int max_n_files = 0, n_cache_trigger = 0, max_mb_usage = lg->max_server_size;
    linked_list_t *files_remained = ll_create();
    int curr_n_files = 0;

    printf("Printing full log:\n");

    node_t* curr_node = ll_get_head_node(lg->entry_list);
    while(curr_node != NULL)
    {
        logging_entry_t* curr_en = node_get_value(curr_node);
        
        switch (curr_en->type)
        {
        case CLIENT_JOINED:
            printf("[ClientJoin] Client %d joined the server.\n", curr_en->fd);
            break;
        case CLIENT_LEFT:
            printf("[ClientLeft] Client %d left the server.\n", curr_en->fd);
            break;
        case FILE_OPENED:
            printf("[FileOpen] File %s has been opened by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_APPEND:
            printf("[FileAppend] File %s has been appended by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_WROTE:
            printf("[FileWrite] File %s has been wrote by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_ADDED:
            curr_n_files += 1;
            if(curr_n_files > max_n_files)
                max_n_files = curr_n_files;

            int res = ll_contains_str(files_remained, curr_en->value);
            if(res == 0)
                ll_add_tail(files_remained, curr_en->value);

            printf("[FileAdd] File %s has been added by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_CLOSED:
            printf("[FileClose] File %s has been closed by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_REMOVED:
            curr_n_files -= 1;
            ll_remove_str(files_remained, curr_en->value);
            printf("[FileRemove] File %s has been removed by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_READ:
            printf("[FileRead] File %s has been read by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        case FILE_NREAD:
            printf("[FileNRead] %d file has been read by %d.\n", *(int*)curr_en->value, curr_en->fd);
            break;
        case CACHE_REPLACEMENT:
            n_cache_trigger += 1;
            ll_remove_str(files_remained, curr_en->value);
            printf("[CacheReplacement] File %s has been removed due to capability issues, triggered by %d.\n", (char*)curr_en->value, curr_en->fd);
            break;
        default:
            break;
        }

        curr_node = node_get_next(curr_node);
    }

    printf("End full log, printing some quick numbers:\n");
    printf("Max number of files stored: %d.\n", max_n_files);
    printf("Max memory used in MB: %d.\n", max_mb_usage);
    printf("Total cache replacement: %d.\n", n_cache_trigger);
    printf("END.\n");
}