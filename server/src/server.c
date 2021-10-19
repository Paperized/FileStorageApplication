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

typedef enum {
    R_ADD_CLIENT,
    R_CHECK_FLAG
} notification_t;

typedef struct server {
    int server_socket_id;

    configuration_params_t* config;
    pthread_mutex_t config_mutex;

    quit_signal_t quit_signal;
    pthread_mutex_t quit_signal_mutex;

    pthread_cond_t clients_pending_cond;
    pthread_mutex_t clients_pending_mutex;
    queue_t* clients_pending;

    fd_set clients_set_connected;
    int clients_set_max_id;
    pthread_mutex_t clients_set_connected_mutex;

    pthread_mutex_t clients_count_mutex;
    unsigned int clients_count;

    logging_t* logging;
    file_system_t* fs;
    int pipe_connections_handler[2];
} server_t;

server_t* singleton_server = NULL;

/** VARIABLE STATUS **/
static bool_t socket_initialized = FALSE;
static bool_t workers_initialized = FALSE;
static bool_t signals_initialized = FALSE;
static bool_t connections_handler_initialized = FALSE;

/** THREAD IDS **/
static pthread_t* thread_workers_ids;
static pthread_t thread_connections_id;

static unsigned int workers_count = 0;
static unsigned int max_client_alltogether = 0;

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    LOG_EVENT("Server functionality failed! (" #initializer ")", -1); \
                                                                    SET_VAR_MUTEX(singleton_server->quit_signal, S_FAST, &singleton_server->quit_signal_mutex); \
                                                                    server_wait_for_threads(); \
                                                                    server_cleanup(); \
                                                                    return status; \
                                                                }

#define __BREAK_ON_CLOSE(b_output, count, m) if((b_output = threads_must_close_util(count))) \
                                            { \
                                                UNLOCK_MUTEX(m); \
                                                break; \
                                            }

#define BREAK_ON_CLOSE_CONDITION_MUTEX(b_output, m)  { \
                                                    int _curr_clients = 0; \
                                                    GET_VAR_MUTEX(singleton_server->clients_count, _curr_clients, &singleton_server->clients_count_mutex); \
                                                    __BREAK_ON_CLOSE(b_output, _curr_clients, m); \
                                                }

#define BREAK_ON_CLOSE_CONDITION(b_output, m)  { \
                                                    if((b_output = threads_must_close_util(singleton_server->clients_count))) \
                                                    __BREAK_ON_CLOSE(b_output, singleton_server->clients_count, m); \
                                                }

#define ll_add_head(ll, val) ll_add_head(ll, val); if(strcmp(#ll, "singleton_server->clients_connected") == 0) PRINT_WARNING(0, "[ADD] n°%d", ll_count(ll));

static inline bool_t threads_must_close_util(int curr_clients)
{
    quit_signal_t signal;
    GET_VAR_MUTEX(singleton_server->quit_signal, signal, &singleton_server->quit_signal_mutex);

    if(signal == S_FAST)
        return TRUE;
    else if(signal == S_NONE)
        return FALSE;

    return curr_clients <= 0;
}

static inline bool_t threads_must_close()
{
    quit_signal_t signal;
    GET_VAR_MUTEX(singleton_server->quit_signal, signal, &singleton_server->quit_signal_mutex);

    if(signal == S_FAST)
        return TRUE;
    else if(signal == S_NONE)
        return FALSE;

    int _curr_clients = 0;
    GET_VAR_MUTEX(singleton_server->clients_count, _curr_clients, &singleton_server->clients_count_mutex);
    return _curr_clients <= 0;
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
    notification_t type = R_ADD_CLIENT;
    char msg_add_client[sizeof(notification_t) + sizeof(int)];
    memcpy(msg_add_client, &type, sizeof(notification_t));

    bool_t must_close = FALSE;
    while(!threads_must_close())
    {
        LOCK_MUTEX(&singleton_server->clients_pending_mutex);
        while(count_q(singleton_server->clients_pending) == 0)
        {
            COND_WAIT(&singleton_server->clients_pending_cond, &singleton_server->clients_pending_mutex);
            BREAK_ON_CLOSE_CONDITION_MUTEX(must_close, &singleton_server->clients_pending_mutex);
        }

        // quit worker loop if signal
        if(must_close)
            break;

        int* client_pending_ptr = (int*)dequeue(singleton_server->clients_pending);
        UNLOCK_MUTEX(&singleton_server->clients_pending_mutex);

        int client_pending = *client_pending_ptr;
        free(client_pending_ptr);

        if(client_pending == -1)
            break;

        server_packet_op_t request_op;
        // on connection closed
        if(readn(client_pending, &request_op, sizeof(server_packet_op_t)) == 0)
        {
            LOCK_MUTEX(&singleton_server->clients_set_connected_mutex);
            SET_VAR_MUTEX(singleton_server->clients_count, singleton_server->clients_count - 1, &singleton_server->clients_count_mutex);
            UNLOCK_MUTEX(&singleton_server->clients_set_connected_mutex);

            acquire_write_lock_fs(singleton_server->fs);
            notify_client_disconnected_fs(singleton_server->fs, client_pending);
            release_write_lock_fs(singleton_server->fs);
            
            LOG_EVENT("OP_CLOSE_CONN client disconnected with id %d", -1, client_pending);
            continue;
        }

        if(!is_valid_op(request_op))
        {
            PRINT_ERROR_DEBUG(EINVAL, "Operation not valid?? %d", (int)request_op);
            close(client_pending);
            continue;
        }

        PRINT_INFO_DEBUG("[W/%lu] Handling client with id %d.", curr, client_pending);

        int res;
        // Handle the message
        switch(request_op)
        {
            case OP_OPEN_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_OPEN_FILE request operation.", curr);
                res = handle_open_file_req(client_pending);
                break;

            case OP_LOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_LOCK_FILE request operation.", curr);
                res = handle_lock_file_req(client_pending);
                break;

            case OP_UNLOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_UNLOCK_FILE request operation.", curr);
                res = handle_unlock_file_req(client_pending);
                break;

            case OP_REMOVE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_REMOVE_FILE request operation.", curr);
                res = handle_remove_file_req(client_pending);
                break;

            case OP_WRITE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_WRITE_FILE request operation.", curr);
                res = handle_write_file_req(client_pending);
                break;

            case OP_APPEND_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_APPEND_FILE request operation.", curr);
                res = handle_append_file_req(client_pending);
                break;
            
            case OP_READ_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_READ_FILE request operation.", curr);
                res = handle_read_file_req(client_pending);
                break;

            case OP_READN_FILES:
                PRINT_INFO_DEBUG("[W/%lu] OP_READN_FILES request operation.", curr);
                res = handle_nread_files_req(client_pending);
                break;

            case OP_CLOSE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_CLOSE_FILE request operation.", curr);
                res = handle_close_file_req(client_pending);
                break;

            default:
                PRINT_INFO_DEBUG("[W/%lu] Unknown request operation, skipping request.", curr);
                res = -1;
                break;
        }

        notify_worker_handled_req_fs(get_fs(), curr);
        // res == -1 doesn't send any packet, a future request will take care of this (e.g. locks)
        if(res > 0)
        {
            server_packet_op_t res_op = OP_ERROR;
            if(writen(client_pending, &res_op, sizeof(res_op)))
                writen(client_pending, &res, sizeof(res));
        }

        PRINT_INFO_DEBUG("[W/%lu] Finished handling.", curr);

        memcpy(msg_add_client + sizeof(notification_t), &client_pending, sizeof(int));
        write(singleton_server->pipe_connections_handler[1], msg_add_client, sizeof(msg_add_client));
    }

    // on close
    PRINT_INFO_DEBUG("Quitting worker.");
    LOG_EVENT("Quitting thread worker PID: %lu", -1, curr);
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
    CHECK_ERROR_EQ(error, pthread_sigmask(SIG_BLOCK, &managed_signals, NULL), -1, S_FAST, "Couldn't set new sigmask!");

    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(sig_act));
    sig_act.sa_handler = SIG_IGN;
    CHECK_ERROR_EQ(error, sigaction(SIGPIPE, &sig_act, NULL), -1, S_FAST, "Couldn't ignore pipe signal!");

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
            LOG_EVENT("External signal received: S_SOFT", -1);
            break;

        case S_FAST:
            PRINT_INFO_DEBUG("\nQuitting with signal: S_FAST.");
            LOG_EVENT("External signal received: S_FAST", -1);
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
        LOG_EVENT("Created new thread worker! PID: %lu", -1, thread_workers_ids[i]);
        workers_count += 1;
    }

    workers_initialized = TRUE;
    return SERVER_OK;
}

void* handle_connections(void* params)
{
    FD_ZERO(&singleton_server->clients_set_connected);
    FD_SET(singleton_server->server_socket_id, &singleton_server->clients_set_connected);
    FD_SET(singleton_server->pipe_connections_handler[0], &singleton_server->clients_set_connected);
    singleton_server->clients_set_max_id = MAX(singleton_server->server_socket_id, singleton_server->pipe_connections_handler[0]);

    bool_t soft_close_in_progress = FALSE;
    while(!threads_must_close())
    {
        fd_set current_set;
        int max_fds;

        LOCK_MUTEX(&singleton_server->clients_set_connected_mutex);
        current_set = singleton_server->clients_set_connected;
        max_fds = singleton_server->clients_set_max_id;
        UNLOCK_MUTEX(&singleton_server->clients_set_connected_mutex);

        int res = select(max_fds + 1, &current_set, NULL, NULL, NULL);
        if(res <= 0)
            continue;
        if(FD_ISSET(singleton_server->pipe_connections_handler[0], &current_set))
        {
            notification_t type;
            read(singleton_server->pipe_connections_handler[0], &type, sizeof(notification_t));

            if(type == R_CHECK_FLAG)
            {
                quit_signal_t sgn;
                read(singleton_server->pipe_connections_handler[0], &sgn, sizeof(quit_signal_t));
                if(sgn == S_FAST)
                    break;
                else if(sgn == S_SOFT)
                {
                    soft_close_in_progress = TRUE;
                }
            }
            else if(type == R_ADD_CLIENT)
            {
                int fd;
                read(singleton_server->pipe_connections_handler[0], &fd, sizeof(int));
                LOCK_MUTEX(&singleton_server->clients_set_connected_mutex);
                FD_SET(fd, &singleton_server->clients_set_connected);
                singleton_server->clients_set_max_id = MAX(fd, singleton_server->clients_set_max_id);
                UNLOCK_MUTEX(&singleton_server->clients_set_connected_mutex);
            }

            --res;
        }

        if(FD_ISSET(singleton_server->server_socket_id, &current_set))
        {
            int new_id = accept(singleton_server->server_socket_id, NULL, 0);
            if(new_id != -1)
            {
                if(soft_close_in_progress)
                {
                    close(new_id);
                }
                else
                {
                    LOCK_MUTEX(&singleton_server->clients_set_connected_mutex);
                    FD_SET(new_id, &singleton_server->clients_set_connected);
                    singleton_server->clients_set_max_id = MAX(new_id, singleton_server->clients_set_max_id);
                    UNLOCK_MUTEX(&singleton_server->clients_set_connected_mutex);

                    LOG_EVENT("OP_CONN client connected with id %d!", -1, new_id);

                    // aggiorno eventualmente il massimo num di client concorrenti
                    int n_clients;
                    GET_VAR_MUTEX(singleton_server->clients_count, n_clients, &singleton_server->clients_count_mutex);
                    if(n_clients + 1 > max_client_alltogether)
                        max_client_alltogether = n_clients;
                }
            }
            --res;
        }
        
        while(res > 0 && max_fds >= 0)
        {
            if(max_fds == singleton_server->server_socket_id || max_fds == singleton_server->pipe_connections_handler[0])
            {
                --max_fds;
                continue;
            }

            if(FD_ISSET(max_fds, &current_set))
            {
                int* new_client;
                MAKE_COPY(new_client, int, max_fds);

                int num_reqs = 0;

                EXEC_WITH_MUTEX(FD_CLR(max_fds, &singleton_server->clients_set_connected), &singleton_server->clients_set_connected_mutex);

                LOCK_MUTEX(&singleton_server->clients_pending_mutex);
                enqueue(singleton_server->clients_pending, new_client);
                num_reqs = count_q(singleton_server->clients_pending);

                if(num_reqs == 1)
                    COND_SIGNAL(&singleton_server->clients_pending_cond);
                UNLOCK_MUTEX(&singleton_server->clients_pending_mutex);
                --res;
            }

            --max_fds;
        }
    }

    LOG_EVENT("Quitting thread accepter! PID: %lu", -1, pthread_self());
    return NULL;
}

int initialize_connection_accepter()
{
    int error;
    CHECK_ERROR_NEQ(error, pthread_create(&thread_connections_id, NULL, &handle_connections, NULL), 0, ERR_SOCKET_INIT_ACCEPTER, THREAD_CREATE_FATAL);

    LOG_EVENT("Created new thread accepter! PID: %lu", -1, thread_connections_id);
    connections_handler_initialized = TRUE;
    return SERVER_OK;
}

int server_wait_for_threads()
{
    quit_signal_t closing_signal;

    // wait for a closing signal
    GET_VAR_MUTEX(singleton_server->quit_signal, closing_signal, &singleton_server->quit_signal_mutex);

    notification_t type = R_CHECK_FLAG;
    char msg_check_flag[sizeof(notification_t) + sizeof(quit_signal_t)];
    memcpy(msg_check_flag, &type, sizeof(notification_t));
    memcpy(msg_check_flag + sizeof(notification_t), &closing_signal, sizeof(quit_signal_t));

    write(singleton_server->pipe_connections_handler[1], msg_check_flag, sizeof(msg_check_flag));
    PRINT_INFO_DEBUG("Joining accepter thread.");
    pthread_join(thread_connections_id, NULL);

    COND_BROADCAST(&singleton_server->clients_pending_cond);

    PRINT_INFO_DEBUG("Joining workers thread.");
    for(int i = 0; i < workers_count; ++i)
    {
        pthread_join(thread_workers_ids[i], NULL);
    }

    close(singleton_server->server_socket_id);
    // log max clients simultaniously (max_clients_alltoghether)
    LOG_EVENT("FINAL_METRICS Max clients connected alltogether %u!", -1, max_client_alltogether);

    // log fs metrics
    shutdown_fs(singleton_server->fs);
    stop_log(singleton_server->logging);
    return SERVER_OK;
}

int server_cleanup()
{
    PRINT_INFO("Freeing memory.");

    close(singleton_server->pipe_connections_handler[0]);
    close(singleton_server->pipe_connections_handler[1]);

    free_fs(singleton_server->fs);
    free_log(singleton_server->logging);
    free_q(singleton_server->clients_pending, free);
    free(thread_workers_ids);

    PRINT_INFO("Closing socket and removing it.");

    pthread_mutex_destroy(&singleton_server->config_mutex);
    pthread_mutex_destroy(&singleton_server->quit_signal_mutex);
    pthread_mutex_destroy(&singleton_server->clients_set_connected_mutex);
    pthread_mutex_destroy(&singleton_server->clients_count_mutex);
    pthread_mutex_destroy(&singleton_server->clients_pending_mutex);
    pthread_cond_destroy(&singleton_server->clients_pending_cond);

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

    pipe(singleton_server->pipe_connections_handler);

    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_accepter, lastest_status);

    // needed for threads metrics
    set_workers_fs(singleton_server->fs, thread_workers_ids, workers_count);

    PRINT_INFO("Server started with PID:%d.", getpid());
    LOG_EVENT("Server started succesfully! PID: %d", -1, getpid());

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
    INIT_MUTEX(&singleton_server->clients_count_mutex);
    INIT_MUTEX(&singleton_server->clients_set_connected_mutex);
    INIT_MUTEX(&singleton_server->clients_pending_mutex);
    INIT_COND(&singleton_server->clients_pending_cond);

    singleton_server->clients_pending = create_q();

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

    LOG_EVENT("Server initialized succesfully!", -1);
    socket_initialized = TRUE;
    return SERVER_OK;
}