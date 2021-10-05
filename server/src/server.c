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

    logging_t* logging;
    file_system_t* fs;
} server_t;

server_t* singleton_server = NULL;

/** VARIABLE STATUS **/
static bool_t socket_initialized = FALSE;
static bool_t workers_initialized = FALSE;
static bool_t signals_initialized = FALSE;
static bool_t connection_accepter_initialized = FALSE;
static bool_t reader_initialized = FALSE;

/** THREAD IDS **/
static pthread_t* thread_workers_ids;
static pthread_t thread_accepter_id;
static pthread_t thread_reader_id;

static unsigned int workers_count = 0;

pthread_mutex_t curr_client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int curr_client_connected  = 0;

static unsigned int max_client_alltogether = 0;

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    LOG_EVENT("Server functionality failed! (" #initializer ")"); \
                                                                    SET_VAR_MUTEX(singleton_server->quit_signal, S_FAST, &singleton_server->quit_signal_mutex); \
                                                                    server_wait_for_threads(); \
                                                                    server_cleanup(); \
                                                                    return status; \
                                                                }

#define BREAK_ON_CLOSE_CONDITION(b_output, m)  if((b_output = threads_must_close())) \
                                                { \
                                                    UNLOCK_MUTEX(m); \
                                                    break; \
                                                }

static inline bool_t threads_must_close()
{
    quit_signal_t signal;
    GET_VAR_MUTEX(singleton_server->quit_signal, signal, &singleton_server->quit_signal_mutex);

    if(signal == S_FAST)
        return TRUE;
    else if(signal == S_NONE)
        return FALSE;

    unsigned int clients_connected;
    GET_VAR_MUTEX(curr_client_connected, clients_connected, &curr_client_count_mutex);
    return clients_connected <= 0;
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

logging_t* get_log()
{
    return singleton_server->logging;
}

void* handle_client_requests(void* data)
{
    pthread_t curr = pthread_self();

    PRINT_INFO_DEBUG("Running worker thread %lu.", curr);
    bool_t must_close = FALSE;
    while(!threads_must_close())
    {
        LOCK_MUTEX(&singleton_server->requests_queue_mutex);
        while(count_q(singleton_server->requests_queue) == 0)
        {
            COND_WAIT(&singleton_server->request_received_cond, &singleton_server->requests_queue_mutex);
            BREAK_ON_CLOSE_CONDITION(must_close, &singleton_server->requests_queue_mutex);
        }

        // quit worker loop if signal
        if(must_close)
            break;

        packet_t* request = (packet_t*)dequeue(singleton_server->requests_queue);
        UNLOCK_MUTEX(&singleton_server->requests_queue_mutex);

        if(request == NULL)
        {
            PRINT_WARNING_DEBUG(EINVAL, "[W/%lu] Request null, skipping.", curr);
            continue;
        }

        PRINT_INFO_DEBUG("[W/%lu] Handling client with id %d.", curr, packet_get_sender(request));
        packet_t* res_packet = create_packet(OP_OK, 0);

        int res;
        // Handle the message
        switch(packet_get_op(request))
        {
            case OP_OPEN_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_OPEN_FILE request operation.", curr);
                res = handle_open_file_req(request, res_packet);
                break;

            case OP_LOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_LOCK_FILE request operation.", curr);
                res = handle_lock_file_req(request, res_packet);
                break;

            case OP_UNLOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_UNLOCK_FILE request operation.", curr);
                res = handle_unlock_file_req(request, res_packet);
                break;

            case OP_REMOVE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_REMOVE_FILE request operation.", curr);
                res = handle_remove_file_req(request, res_packet);
                break;

            case OP_WRITE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_WRITE_FILE request operation.", curr);
                res = handle_write_file_req(request, res_packet);
                break;

            case OP_APPEND_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_APPEND_FILE request operation.", curr);
                res = handle_append_file_req(request, res_packet);
                break;
            
            case OP_READ_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_READ_FILE request operation.", curr);
                res = handle_read_file_req(request, res_packet);
                break;

            case OP_READN_FILES:
                PRINT_INFO_DEBUG("[W/%lu] OP_READN_FILES request operation.", curr);
                res = handle_nread_files_req(request, res_packet);
                break;

            case OP_CLOSE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_CLOSE_FILE request operation.", curr);
                res = handle_close_file_req(request, res_packet);
                break;

            default:
                PRINT_INFO_DEBUG("[W/%lu] Unknown request operation, skipping request.", curr);
                break;
        }

        notify_worker_handled_req_fs(get_fs(), curr);
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
        PRINT_INFO_DEBUG("[W/%lu] Finished handling.", curr);
    }

    // on close
    PRINT_INFO_DEBUG("Quitting worker.");
    LOG_EVENT("Quitting thread worker PID: %lu", curr);
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

    switch(shared_quit_signal)
    {
        case S_NONE:
            PRINT_INFO_DEBUG("\nQuitting with signal: S_NONE.");
            break;

        case S_SOFT:
            PRINT_INFO_DEBUG("\nQuitting with signal: S_SOFT.");
            LOG_EVENT("External signal received: S_SOFT");
            break;

        case S_FAST:
            PRINT_INFO_DEBUG("\nQuitting with signal: S_FAST.");
            LOG_EVENT("External signal received: S_FAST");
            break;

        default:
            PRINT_INFO_DEBUG("\nQuitting with signal: ??.");
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
        LOG_EVENT("Created new thread worker! PID: %lu", thread_workers_ids[i]);
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

        int new_id = accept(singleton_server->server_socket_id, NULL, 0);
        if(new_id == -1)
            continue;
        
        int* new_client;
        MAKE_COPY(new_client, int, new_id);
        SET_VAR_MUTEX(add_failed, ll_add_head(singleton_server->clients_connected, new_client) != 0, &singleton_server->clients_list_mutex);

        // if something go wrong with the queue we close the connection right away
        if(add_failed)
        {
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

            LOG_EVENT("OP_CONN client connected with id %d!", new_id);

            COND_SIGNAL(&singleton_server->clients_connected_cond);

            // aggiorno il num dei client correnti
            LOCK_MUTEX(&curr_client_count_mutex);
            int num_clients = ++curr_client_connected;
            UNLOCK_MUTEX(&curr_client_count_mutex);

            // aggiorno eventualmente il massimo num di client concorrenti
            if(num_clients > max_client_alltogether)
                max_client_alltogether = num_clients;
        }
    }

    LOG_EVENT("Quitting thread accepter! PID: %lu", pthread_self());
    return NULL;
}

int initialize_connection_accepter()
{
    int error;
    FD_ZERO(&singleton_server->clients_connected_set);
    CHECK_ERROR_NEQ(error, pthread_create(&thread_accepter_id, NULL, &handle_connections, NULL), 0, ERR_SOCKET_INIT_ACCEPTER, THREAD_CREATE_FATAL);

    LOG_EVENT("Created new thread accepter! PID: %lu", thread_accepter_id);
    connection_accepter_initialized = TRUE;
    return SERVER_OK;
}

void* handle_clients_packets()
{
    bool_t must_close = FALSE;
    struct timeval tv;

    while(!threads_must_close())
    {
        // wait 0.1s per check
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        LOCK_MUTEX(&singleton_server->clients_list_mutex);
        size_t n_clients;
        while((n_clients = ll_count(singleton_server->clients_connected)) == 0)
        {
            COND_WAIT(&singleton_server->clients_connected_cond, &singleton_server->clients_list_mutex);
            BREAK_ON_CLOSE_CONDITION(must_close, &singleton_server->clients_list_mutex);
        }

        if(must_close)
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

        while(curr_client)
        {
            int curr_session = *((int*)node_get_value(curr_client));

            if(FD_ISSET(curr_session, &current_clients))
            {
                packet_t* req = read_packet_from_fd(curr_session);

                if(is_packet_valid(req))
                {
                    if(packet_get_op(req) == OP_CLOSE_CONN)
                    {
                        // chiudo la connessione
                        EXEC_WITH_MUTEX(FD_CLR(curr_session, &singleton_server->clients_connected_set), &singleton_server->clients_set_mutex);

                        node_t* temp = node_get_next(curr_client);
                        free(node_get_value(curr_client));
                        ll_remove_node(singleton_server->clients_connected, curr_client);
                        curr_client = temp;

                        acquire_write_lock_fs(singleton_server->fs);
                        notify_client_disconnected_fs(singleton_server->fs, curr_session);
                        release_write_lock_fs(singleton_server->fs);
                        
                        LOG_EVENT("OP_CLOSE_CONN client disconnected with id %d", curr_session);

                        SET_VAR_MUTEX(curr_client_connected, curr_client_connected - 1, &curr_client_count_mutex);

                        destroy_packet(req);
                        continue;
                    }

                    LOCK_MUTEX(&singleton_server->requests_queue_mutex);

                    enqueue(singleton_server->requests_queue, (void*)req);
                    if(count_q(singleton_server->requests_queue) == 1)
                    {
                        COND_SIGNAL(&singleton_server->request_received_cond);
                    }

                    UNLOCK_MUTEX(&singleton_server->requests_queue_mutex);
                }
                else
                    destroy_packet(req);
            }

            curr_client = node_get_next(curr_client);
        }
        UNLOCK_MUTEX(&singleton_server->clients_list_mutex);
    }

    PRINT_INFO("Quitting packet reader.");
    LOG_EVENT("Quitting thread reader! PID: %lu", pthread_self());
    return NULL;
}

int initialize_reader()
{
    int error;
    CHECK_ERROR_NEQ(error, pthread_create(&thread_reader_id, NULL, &handle_clients_packets, NULL), 0, ERR_SERVER_INIT_READER, THREAD_CREATE_FATAL);

    LOG_EVENT("Created new thread reader! PID: %lu", thread_reader_id);
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
        LOG_EVENT("Quitting thread accepter! PID: %lu", thread_accepter_id);
    }

    // The thread might be locked in a wait
    if(closing_signal == S_FAST)
    {
        PRINT_INFO_DEBUG("Joining reader thread.");
        COND_SIGNAL(&singleton_server->clients_connected_cond);
    }

    pthread_join(thread_reader_id, NULL);

    // either is S_FAST or S_SOFT we need to broadcast workers since the reader already finished
    COND_BROADCAST(&singleton_server->request_received_cond);

    PRINT_INFO_DEBUG("Joining workers thread.");
    for(int i = 0; i < workers_count; ++i)
    {
        pthread_join(thread_workers_ids[i], NULL);
    }

    // log max clients simultaniously (max_clients_alltoghether)
    LOG_EVENT("FINAL_METRICS Max clients connected alltogether %u!", max_client_alltogether);

    // log fs metrics
    shutdown_fs(singleton_server->fs);
    stop_log(singleton_server->logging);

    return SERVER_OK;
}

int server_cleanup()
{
    PRINT_INFO("Freeing memory.");

    free_fs(singleton_server->fs);
    free_log(singleton_server->logging);
    ll_free(singleton_server->clients_connected, free);
    free_q(singleton_server->requests_queue, FREE_FUNC(destroy_packet));
    free(thread_workers_ids);
    destroy_packet(p_on_file_deleted_locks);
    destroy_packet(p_on_file_given_lock);

    PRINT_INFO("Closing socket and removing it.");
    close(singleton_server->server_socket_id);

    pthread_mutex_destroy(&singleton_server->config_mutex);
    pthread_mutex_destroy(&singleton_server->quit_signal_mutex);
    pthread_mutex_destroy(&singleton_server->clients_list_mutex);
    pthread_mutex_destroy(&singleton_server->clients_set_mutex);
    pthread_mutex_destroy(&singleton_server->requests_queue_mutex);
    pthread_mutex_destroy(&curr_client_count_mutex);

    pthread_cond_destroy(&singleton_server->clients_connected_cond);
    pthread_cond_destroy(&singleton_server->request_received_cond);

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(singleton_server->config, socket_name);
    remove(socket_name);

    free(singleton_server);
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

    // needed for threads metrics
    set_workers_fs(singleton_server->fs, thread_workers_ids, workers_count);

    PRINT_INFO("Server started with PID:%d.", getpid());
    LOG_EVENT("Server started succesfully! PID: %d", getpid());

    // blocking call, return on quit signal
    server_wait_end_signal();
    server_wait_for_threads();

    return server_cleanup();
}

int init_server(const configuration_params_t* config)
{
    CHECK_FATAL_EQ(singleton_server, malloc(sizeof(server_t)), NULL, NO_MEM_FATAL);
    memset(singleton_server, 0, sizeof(struct server));

    singleton_server->config = (configuration_params_t*)config;
    
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

    singleton_server->logging = create_log();
    
    char log_name[MAX_PATHNAME_API_LENGTH + 1];
    config_get_log_name(singleton_server->config, log_name);

    if(start_log(singleton_server->logging, log_name) == -1)
    {
        PRINT_WARNING(errno, "Cannot prepare logging!");
    }

    char policy[MAX_POLICY_LENGTH + 1];
    config_get_log_name(config, policy);
    set_policy_fs(singleton_server->fs, policy);

    // unique packets used many times
    p_on_file_deleted_locks = create_packet(OP_ERROR, sizeof(int));
    int err_packet = EIDRM;
    write_data(p_on_file_deleted_locks, &err_packet, sizeof(int));
    p_on_file_given_lock = create_packet(OP_OK, sizeof(int));

    CHECK_ERROR_EQ(singleton_server->server_socket_id, socket(AF_UNIX, SOCK_STREAM, 0), -1, ERR_SOCKET_FAILED, "Couldn't initialize socket!");

    char socket_name[MAX_PATHNAME_API_LENGTH + 1];
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

    LOG_EVENT("Server initialized succesfully!");
    socket_initialized = TRUE;
    return SERVER_OK;
}