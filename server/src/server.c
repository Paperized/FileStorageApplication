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

// Enum used to notify the connection handler for an upcoming event
typedef enum {
    R_ADD_CLIENT,
    R_CHECK_FLAG
} notification_t;

// Current server socket fd
static int server_socket_id = -1;

// Configuration used by the server
static configuration_params_t* current_config = NULL;
// config associated mutex
static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;

// Quit signal of the server
static quit_signal_t quit_signal = S_NONE;
// quit_signal associated mutex
static pthread_mutex_t quit_signal_mutex = PTHREAD_MUTEX_INITIALIZER;

// Condition variable on client added in queue (clients_pending)
static pthread_cond_t clients_pending_cond = PTHREAD_COND_INITIALIZER;
// clients_pending associated mutex
static pthread_mutex_t clients_pending_mutex = PTHREAD_MUTEX_INITIALIZER;
// Queue of clients to be handled by workers
static queue_t* clients_pending = NULL;

// Main fd_set used by the connection handler, listens to the socket, pipe and clients
static fd_set clients_set_connected;
// Max fd for clients_set_connected 
static int clients_set_max_id = -1;
// clients_set_connected associated mutex
static pthread_mutex_t clients_set_connected_mutex = PTHREAD_MUTEX_INITIALIZER;

// clients_count associated mutex
static pthread_mutex_t clients_count_mutex = PTHREAD_MUTEX_INITIALIZER;
// Current clients count connected
static unsigned int clients_count = 0;

// Logger
static logging_t* logging = NULL;
// File system
static file_system_t* fs = NULL;
// Pipe connection used to notify the connection handler for any events
static int pipe_connections_handler[2];

// Is server socket initialized
static bool_t socket_initialized = FALSE;
// Are workers initialized
static bool_t workers_initialized = FALSE;
// Is signal handler initialized
static bool_t signals_initialized = FALSE;
// Is connection handler initialized
static bool_t connections_handler_initialized = FALSE;

// Array of pids of workers
static pthread_t* thread_workers_ids;
// pid of connection handler
static pthread_t thread_connections_id;

// Current workers count
static unsigned int workers_count = 0;
// Metrics max clients connected alltogether
static unsigned int max_client_alltogether = 0;

// Used during start_server(), initialize a functionality and if the status value is not SERVER_OK rollback the server and close it
// A server functionality is a function without parameters which return a server status code
#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    LOG_EVENT("Server functionality failed! (" #initializer ")", -1); \
                                                                    SET_VAR_MUTEX(quit_signal, S_FAST, &quit_signal_mutex); \
                                                                    server_join_threads(); \
                                                                    server_cleanup(); \
                                                                    return status; \
                                                                }

// Set the first parameter to TRUE if the thread must close, unlock the second parameter mutex and break
#define BREAK_ON_CLOSE_CONDITION_MUTEX(b_output, m)  { \
                                                        if((b_output = threads_must_close())) \
                                                        { \
                                                            UNLOCK_MUTEX(m); \
                                                            break; \
                                                        } \
                                                    }

// Check if the threads needs to quit
// (Return TRUE if quit signal is S_FAST or S_SOFT with no clients connected)
static inline bool_t threads_must_close()
{
    quit_signal_t signal;
    GET_VAR_MUTEX(quit_signal, signal, &quit_signal_mutex);

    if(signal == S_FAST)
        return TRUE;
    else if(signal == S_NONE)
        return FALSE;

    int _curr_clients = 0;
    GET_VAR_MUTEX(clients_count, _curr_clients, &clients_count_mutex);
    return _curr_clients <= 0;
}

quit_signal_t get_quit_signal()
{
    quit_signal_t result;
    GET_VAR_MUTEX(quit_signal, result, &quit_signal_mutex);
    return result;
}

file_system_t* get_fs()
{
    return fs;
}

logging_t* get_log()
{
    return logging;
}

static void on_client_disconnected(int client, bool_t intentional, int* clients_count_ptr)
{
    if(!clients_count_ptr)
    {
        SET_VAR_MUTEX(clients_count, clients_count - 1, &clients_count_mutex);
    }
    else
    {
        *clients_count_ptr = *clients_count_ptr - 1;
    }

    acquire_write_lock_fs(fs);
    notify_client_disconnected_fs(fs, client);
    release_write_lock_fs(fs);
    
    if(intentional)
    {
        LOG_EVENT("OP_CLOSE_CONN client disconnected with id %d", -1, client);
    }
    else
    {
        LOG_EVENT("OP_CLOSE_CONN client disconnected with id %d for an invalid operation", -1, client);
        close(client);
    }
}

// Routine executed by each worker, reads a client fd from a shared queue and handles the request.
// Stops once the quit signal is S_FAST or S_SOFT with no clients connected
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
        LOCK_MUTEX(&clients_pending_mutex);
        while(count_q(clients_pending) == 0)
        {
            COND_WAIT(&clients_pending_cond, &clients_pending_mutex);
            BREAK_ON_CLOSE_CONDITION_MUTEX(must_close, &clients_pending_mutex);
        }

        // quit worker loop if signal
        if(must_close)
            break;

        int* client_pending_ptr = (int*)dequeue(clients_pending);
        UNLOCK_MUTEX(&clients_pending_mutex);

        int client_pending = *client_pending_ptr;
        free(client_pending_ptr);

        if(client_pending == -1)
            break;

        server_packet_op_t request_op = OP_UNKNOWN;
        bool_t clients_disconnected = readn(client_pending, &request_op, sizeof(server_packet_op_t)) == 0;
        bool_t clients_invalid_req = !is_valid_op(request_op);
        if(clients_disconnected || clients_invalid_req)
        {
            on_client_disconnected(client_pending, clients_disconnected, NULL);
            continue;
        }

        PRINT_INFO_DEBUG("[W/%lu] Handling client with id %d.", curr, client_pending);

        // Handle the message
        switch(request_op)
        {
            case OP_OPEN_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_OPEN_FILE request operation.", curr);
                handle_open_file_req(client_pending);
                break;

            case OP_LOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_LOCK_FILE request operation.", curr);
                handle_lock_file_req(client_pending);
                break;

            case OP_UNLOCK_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_UNLOCK_FILE request operation.", curr);
                handle_unlock_file_req(client_pending);
                break;

            case OP_REMOVE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_REMOVE_FILE request operation.", curr);
                handle_remove_file_req(client_pending);
                break;

            case OP_WRITE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_WRITE_FILE request operation.", curr);
                handle_write_file_req(client_pending);
                break;

            case OP_APPEND_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_APPEND_FILE request operation.", curr);
                handle_append_file_req(client_pending);
                break;
            
            case OP_READ_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_READ_FILE request operation.", curr);
                handle_read_file_req(client_pending);
                break;

            case OP_READN_FILES:
                PRINT_INFO_DEBUG("[W/%lu] OP_READN_FILES request operation.", curr);
                handle_nread_files_req(client_pending);
                break;

            case OP_CLOSE_FILE:
                PRINT_INFO_DEBUG("[W/%lu] OP_CLOSE_FILE request operation.", curr);
                handle_close_file_req(client_pending);
                break;

            default:
                PRINT_INFO_DEBUG("[W/%lu] Unknown request operation, skipping request.", curr);
                break;
        }

        notify_worker_handled_req_fs(get_fs(), curr);
        PRINT_INFO_DEBUG("[W/%lu] Finished handling.", curr);

        memcpy(msg_add_client + sizeof(notification_t), &client_pending, sizeof(int));
        write(pipe_connections_handler[1], msg_add_client, sizeof(msg_add_client));
    }

    // on close
    PRINT_INFO_DEBUG("Quitting worker.");
    LOG_EVENT("Quitting thread worker PID: %lu", -1, curr);
    return NULL;
}

// Called after starting the server by the main thread, waits until a quitting signal is detected
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
        SET_VAR_MUTEX(quit_signal, local_quit_signal, &quit_signal_mutex);
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

// Initialized the workers by creating and execute each of them
int initialize_workers()
{
    CHECK_FATAL_EQ(thread_workers_ids, malloc(config_get_num_workers(current_config) * (sizeof(pthread_t))), NULL, NO_MEM_FATAL);

    int error;
    for(int i = 0; i < config_get_num_workers(current_config); ++i)
    {
        CHECK_ERROR_NEQ(error, pthread_create(&thread_workers_ids[i], NULL, &handle_client_requests, NULL), 0,
                 ERR_SOCKET_INIT_WORKERS, "Coudln't create the %dth thread!", i);
        LOG_EVENT("Created new thread worker! PID: %lu", -1, thread_workers_ids[i]);
        workers_count += 1;
    }

    workers_initialized = TRUE;
    return SERVER_OK;
}

// Routine executed by the connection handler thread, manages the incoming connections and notify the workers about upcoming data
void* handle_connections(void* params)
{
    FD_ZERO(&clients_set_connected);
    FD_SET(server_socket_id, &clients_set_connected);
    FD_SET(pipe_connections_handler[0], &clients_set_connected);
    clients_set_max_id = MAX(server_socket_id, pipe_connections_handler[0]);

    char buffer_check_disconnect[1];
    bool_t soft_close_in_progress = FALSE;
    int curr = 0;
    while(!threads_must_close())
    {
        fd_set current_set;
        int max_fds;

        LOCK_MUTEX(&clients_set_connected_mutex);
        current_set = clients_set_connected;
        max_fds = clients_set_max_id;
        UNLOCK_MUTEX(&clients_set_connected_mutex);

        int res = select(max_fds + 1, &current_set, NULL, NULL, NULL);
        if(res <= 0)
            continue;
        if(FD_ISSET(pipe_connections_handler[0], &current_set))
        {
            notification_t type;
            read(pipe_connections_handler[0], &type, sizeof(notification_t));

            if(type == R_CHECK_FLAG)
            {
                quit_signal_t sgn;
                read(pipe_connections_handler[0], &sgn, sizeof(quit_signal_t));
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
                read(pipe_connections_handler[0], &fd, sizeof(int));
                LOCK_MUTEX(&clients_set_connected_mutex);
                FD_SET(fd, &clients_set_connected);
                clients_set_max_id = MAX(fd, clients_set_max_id);
                UNLOCK_MUTEX(&clients_set_connected_mutex);
            }

            --res;
            if(res == 0)
                continue;
        }

        // Copy of clients_count value, is not initialized yet because of the overhead at every message received
        // It's initialized or inside an incoming connection or inside a disconnection lazily
        int n_clients = -1;
        int n_clients_start = -1;

        if(res > 0 && FD_ISSET(server_socket_id, &current_set))
        {
            int new_id = accept(server_socket_id, NULL, 0);
            if(new_id != -1)
            {
                if(soft_close_in_progress)
                {
                    close(new_id);
                }
                else
                {
                    LOCK_MUTEX(&clients_set_connected_mutex);
                    FD_SET(new_id, &clients_set_connected);
                    clients_set_max_id = MAX(new_id, clients_set_max_id);
                    UNLOCK_MUTEX(&clients_set_connected_mutex);

                    LOG_EVENT("OP_CONN client connected with id %d!", -1, new_id);

                    GET_VAR_MUTEX(clients_count, n_clients_start, &clients_count_mutex);
                    n_clients = n_clients_start + 1;
                }
            }
            --res;
        }
        
        while(res > 0 && max_fds >= 0)
        {
            if(max_fds == server_socket_id || max_fds == pipe_connections_handler[0])
            {
                --max_fds;
                continue;
            }

            if(FD_ISSET(max_fds, &current_set))
            {
                EXEC_WITH_MUTEX(FD_CLR(max_fds, &clients_set_connected), &clients_set_connected_mutex);

                // Read the first unused byte from the client, used to detect whether the client is still connected
                if(read(max_fds, &buffer_check_disconnect, 1) <= 0)
                {
                    // If the n_clients value was not loaded previously load it
                    if(n_clients == -1)
                    {
                        GET_VAR_MUTEX(clients_count, n_clients_start, &clients_count_mutex);
                        n_clients = n_clients_start;
                    }
                    // update the clients count based on this local variable not the global one
                    // the count will be set globally at the end
                    on_client_disconnected(max_fds, TRUE, &n_clients);
                    --res;
                    continue;
                }

                int* new_client;
                MAKE_COPY(new_client, int, max_fds);

                int num_reqs = 0;

                LOCK_MUTEX(&clients_pending_mutex);
                enqueue(clients_pending, new_client);
                num_reqs = count_q(clients_pending);

                if(num_reqs == 1)
                    COND_SIGNAL(&clients_pending_cond);
                UNLOCK_MUTEX(&clients_pending_mutex);
                --res;
            }

            --max_fds;
        }

        // if n_clients was changed by a connection or disconnection
        if(n_clients > -1)
        {
            int clients_count_check;
            GET_VAR_MUTEX(clients_count, clients_count_check, &clients_count_mutex);
            // Check if some workers closed an invalid connection since n_clients_start was set
            if(clients_count_check != n_clients_start)
            {
                // Calculate how many clients they closed and subtract it from the current n_clients calculated in this thread
                int diff = n_clients_start - clients_count_check;
                n_clients -= diff;
            }

            // Set the update clients count back
            SET_VAR_MUTEX(clients_count, n_clients, &clients_count_mutex);
            // and the value is > then the max set it
            if(n_clients > max_client_alltogether)
                max_client_alltogether = n_clients;
        }
    }

    LOG_EVENT("Quitting thread accepter! PID: %lu", -1, pthread_self());
    return NULL;
}

// Initialize the connection handler by executing it's dedicated thread
static int initialize_connection_handler()
{
    int error;
    CHECK_ERROR_NEQ(error, pipe(pipe_connections_handler), 0, ERR_SOCKET_INIT_ACCEPTER, "Cannot initialize pipe!");
    CHECK_ERROR_NEQ(error, pthread_create(&thread_connections_id, NULL, &handle_connections, NULL), 0, ERR_SOCKET_INIT_ACCEPTER, THREAD_CREATE_FATAL);

    LOG_EVENT("Created new thread accepter! PID: %lu", -1, thread_connections_id);
    connections_handler_initialized = TRUE;
    return SERVER_OK;
}

// Called after receiving a S_SOFT or S_FAST from server_wait_end_signal() method
static int server_join_threads()
{
    quit_signal_t closing_signal;

    // wait for a closing signal
    GET_VAR_MUTEX(quit_signal, closing_signal, &quit_signal_mutex);

    notification_t type = R_CHECK_FLAG;
    char msg_check_flag[sizeof(notification_t) + sizeof(quit_signal_t)];
    memcpy(msg_check_flag, &type, sizeof(notification_t));
    memcpy(msg_check_flag + sizeof(notification_t), &closing_signal, sizeof(quit_signal_t));

    write(pipe_connections_handler[1], msg_check_flag, sizeof(msg_check_flag));
    PRINT_INFO_DEBUG("Joining connection handler thread.");
    pthread_join(thread_connections_id, NULL);

    COND_BROADCAST(&clients_pending_cond);

    PRINT_INFO_DEBUG("Joining workers thread.");
    for(int i = 0; i < workers_count; ++i)
    {
        pthread_join(thread_workers_ids[i], NULL);
    }

    close(server_socket_id);
    // log max clients simultaniously (max_clients_alltoghether)
    LOG_EVENT("FINAL_METRICS Max clients connected alltogether %u!", -1, max_client_alltogether);

    // log fs metrics
    shutdown_fs(fs);
    stop_log(logging);
    return SERVER_OK;
}

// Cleanup all memory allocated by the server
int server_cleanup()
{
    PRINT_INFO("Freeing memory.");

    close(pipe_connections_handler[0]);
    close(pipe_connections_handler[1]);

    free_fs(fs);
    free_log(logging);
    free_q(clients_pending, free);
    free(thread_workers_ids);

    PRINT_INFO("Closing socket and removing it.");

    pthread_mutex_destroy(&config_mutex);
    pthread_mutex_destroy(&quit_signal_mutex);
    pthread_mutex_destroy(&clients_set_connected_mutex);
    pthread_mutex_destroy(&clients_count_mutex);
    pthread_mutex_destroy(&clients_pending_mutex);
    pthread_cond_destroy(&clients_pending_cond);

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(current_config, socket_name);
    remove(socket_name);

    // this function returns only SERVER_OK but can be expanded with other return values
    return SERVER_OK;
}

int start_server()
{
    int lastest_status;

    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_handler, lastest_status);

    // needed for threads metrics
    set_workers_fs(fs, thread_workers_ids, workers_count);

    PRINT_INFO("Server started with PID:%d.", getpid());
    LOG_EVENT("Server started succesfully! PID: %d", -1, getpid());

    // blocking call, return on quit signal
    server_wait_end_signal();
    server_join_threads();

    return server_cleanup();
}

int init_server(const configuration_params_t* config)
{
    current_config = (configuration_params_t*)config;

    clients_pending = create_q();

    fs = create_fs(config_get_max_server_size(config),
                                     config_get_max_files_count(config));

    logging = create_log();
    
    char log_name[MAX_PATHNAME_API_LENGTH + 1];
    config_get_log_name(config, log_name);

    if(start_log(logging, log_name) == -1)
    {
        PRINT_WARNING(errno, "Cannot prepare logging!");
    }

    char policy[MAX_POLICY_LENGTH + 1];
    config_get_log_name(config, policy);
    set_policy_fs(fs, policy);

    CHECK_ERROR_EQ(server_socket_id, socket(AF_UNIX, SOCK_STREAM, 0), -1, ERR_SOCKET_FAILED, "Couldn't initialize socket!");

    char socket_name[MAX_PATHNAME_API_LENGTH + 1];
    config_get_socket_name(config, socket_name);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, socket_name, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    remove(socket_name);

    int error;
    CHECK_ERROR_EQ(error,
                    bind(server_socket_id, (struct sockaddr*)&sa, sizeof(sa)), -1,
                    ERR_SOCKET_BIND_FAILED, "Couldn't bind socket!");
    CHECK_ERROR_EQ(error,
                    listen(server_socket_id, config_get_backlog_sockets_num(config)), -1,
                    ERR_SOCKET_LISTEN_FAILED, "Couldn't listen socket!");

    LOG_EVENT("Server initialized succesfully!", -1);
    socket_initialized = TRUE;
    return SERVER_OK;
}