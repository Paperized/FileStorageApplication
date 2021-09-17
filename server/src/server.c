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
#include "replacement_policy.h"

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    SET_VAR_MUTEX(quit_signal, S_FAST, &quit_signal_mutex); \
                                                                    server_wait_for_threads(); \
                                                                    server_cleanup(); \
                                                                    return status; \
                                                                }

#define CHECK_ERROR(boolean, returned_error) if(boolean) return returned_error

#define BREAK_ON_FAST_QUIT(qv, m)  if((qv = get_quit_signal()) == S_FAST) \
                                    { \
                                        pthread_mutex_unlock(m); \
                                        break; \
                                    }


pthread_mutex_t loaded_configuration_mutex = PTHREAD_MUTEX_INITIALIZER;
configuration_params_t* loaded_configuration;
int server_socket_id;

pthread_mutex_t server_log_mutex = PTHREAD_MUTEX_INITIALIZER;
logging_t* server_log;

pthread_mutex_t clients_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_set_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t clients_connected_cond = PTHREAD_COND_INITIALIZER;
linked_list_t* clients_connected;
fd_set clients_connected_set;

pthread_cond_t request_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t requests_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
queue_t* requests_queue;

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
pthread_t thread_accepter_id;
pthread_t thread_reader_id;

pthread_mutex_t current_used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;
size_t current_used_memory = 0;
unsigned int workers_count = 0;

int (*server_policy)(file_stored_t* f1, file_stored_t* f2);

void initialize_policy_delegate()
{
    // pick a policy, default is fifo
    char policy[MAX_POLICY_LENGTH];
    config_get_policy_name(loaded_configuration, policy);

    if(strncmp(policy, "FIFO", 4) == 0)
        server_policy = replacement_policy_fifo;
    else if(strncmp(policy, "LRU", 3) == 0)
        server_policy = replacement_policy_lru;
    else if(strncmp(policy, "LFU", 3) == 0)
        server_policy = replacement_policy_lfu;
    else
        server_policy = replacement_policy_fifo;
}

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

    free(file);
}

int get_max_fid_sessions()
{
    int max = 0;
    node_t* curr = ll_get_head_node(clients_connected);
    if(curr == NULL)
        return max;

    while(curr != NULL)
    {
        client_session_t* session = node_get_value(curr);
        if(session->fd > max)
            max = session->fd;

        curr = node_get_next(curr);
    }

    return max;
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
    while((quit_signal = get_quit_signal()) != S_FAST)
    {
        LOCK_MUTEX(&requests_queue_mutex);
        while(count_q(requests_queue) == 0)
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

        packet_t* request = cast_to(packet_t*, dequeue(requests_queue));
        UNLOCK_MUTEX(&requests_queue_mutex);

        if(request == NULL)
        {
            printf("[W/%lu]: Request null, skipping.\n", curr);
            continue;
        }

        printf("[W/%lu] Handling client with id: %d.\n", curr, packet_get_sender(request));

        int res;
        // Handle the message
        switch(packet_get_op(request))
        {
            case OP_OPEN_FILE:
                res = handle_open_file_req(request, curr);
                break;

            case OP_REMOVE_FILE:
                res = handle_remove_file_req(request, curr);
                break;

            case OP_WRITE_FILE:
                res = handle_write_file_req(request, curr);
                break;

            case OP_APPEND_FILE:
                res = handle_append_file_req(request, curr);
                break;
            
            case OP_READ_FILE:
                res = handle_read_file_req(request, curr);
                break;

            case OP_READN_FILES:
                res = handle_nread_files_req(request, curr);
                break;

            case OP_CLOSE_FILE:
                res = handle_close_file_req(request, curr);
                break;

            default:
                printf("[W/%lu] Unknown request operation, skipping request.\n", curr);
                break;
        }

        // IF RES == -1 SEND BACK AN ERROR MESSAGE WITH ERROR TAKEN FROM ERRNO VAR
        //HANDLE_ERROR(req)

        destroy_packet(request);
        printf("[W/%lu] Finished handling.\n", curr);
    }

    // on close
    printf("Quitting worker.\n");
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
        CHECK_ERROR(sigwait(&managed_signals, &last_signal) != 0, S_FAST);

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

    // debug
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

    return shared_quit_signal;
}

int initialize_workers()
{
    CHECK_FATAL_EQ(thread_workers_ids, malloc(config_get_num_workers(loaded_configuration) * (sizeof(pthread_t))), NULL, NO_MEM_FATAL);

    int error;
    for(int i = 0; i < config_get_num_workers(loaded_configuration); ++i)
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

        PRINT_INFO("Waiting for new connections.\n");
        int new_id = accept(server_socket_id, NULL, 0);
        if(new_id == -1)
            continue;

        // add to queue

        client_session_t* new_client;
        CHECK_FATAL_EQ(new_client, malloc(sizeof(client_session_t)), NULL, NO_MEM_FATAL);
        memset(new_client, 0, sizeof(client_session_t));
        new_client->fd = new_id;
        
        SET_VAR_MUTEX(add_failed, ll_add_head(clients_connected, new_client) != 0, &clients_list_mutex);

        // if something go wrong with the queue we close the connection right away
        if(add_failed)
        {
            PRINT_WARNING(errno, "ll_add_head failed!.\n");
            free(new_client);
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

            EXEC_WITH_MUTEX(add_logging_entry(server_log, CLIENT_JOINED, new_id, NULL), &server_log_mutex);

            COND_SIGNAL(&clients_connected_cond);
        }
    }

    return NULL;
}

int initialize_connection_accepter()
{
    int error;
    FD_ZERO(&clients_connected_set);
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

        LOCK_MUTEX(&clients_list_mutex);
        size_t n_clients;
        while((n_clients = ll_count(clients_connected)) == 0)
        {
            COND_WAIT(&clients_connected_cond, &clients_list_mutex);

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
        node_t* curr_client = ll_get_head_node(clients_connected);

        while(curr_client != NULL)
        {
            client_session_t* curr_session = node_get_value(curr_client);

            if(FD_ISSET(curr_session->fd, &current_clients))
            {
                printf("reading packet %d.\n", curr_session->fd);
                packet_t* req = read_packet_from_fd(curr_session->fd);

                if(is_packet_valid(req))
                {
                    if(packet_get_op(req) == OP_CLOSE_CONN)
                    {
                        // chiudo la connessione
                        EXEC_WITH_MUTEX(FD_CLR(curr_session->fd, &clients_connected_set), &clients_set_mutex);

                        node_t* temp = node_get_next(curr_client);
                        ll_remove_node(clients_connected, curr_client);

                        if(curr_session->prev_file_opened != NULL)
                            free(curr_session->prev_file_opened);
                        
                        while(ll_count(curr_session->files_opened) > 0)
                        {
                            ll_remove_last(curr_session->files_opened, NULL);
                        }

                        curr_client = temp;
                        EXEC_WITH_MUTEX(add_logging_entry(server_log, CLIENT_LEFT, curr_session->fd, NULL), &server_log_mutex);

                        free(curr_session);
                        free(req);
                        continue;
                    }

                    LOCK_MUTEX(&requests_queue_mutex);

                    enqueue_m(requests_queue, req);
                    if(count_q(requests_queue) == 1)
                    {
                        COND_SIGNAL(&request_received_cond);
                    }

                    UNLOCK_MUTEX(&requests_queue_mutex);
                }

                // we ignore failed packets returned by the read
            }

            curr_client = node_get_next(curr_client);
        }
        UNLOCK_MUTEX(&clients_list_mutex);
    }

    printf("Quitting packet reader.\n");
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
    GET_VAR_MUTEX(quit_signal, closing_signal, &quit_signal_mutex);

    if(connection_accepter_initialized)
    {
        pthread_cancel(thread_accepter_id);
        pthread_join(thread_accepter_id, NULL);
    }

    if(closing_signal == S_FAST)
    {
        COND_BROADCAST(&request_received_cond);
    }
        

    printf("Joining workers thread.\n");
    for(int i = 0; i < workers_count; ++i)
    {
        //if(closing_signal == S_FAST)
        //    pthread_cancel(thread_workers_ids[i]);

        pthread_join(thread_workers_ids[i], NULL);
    }

    // The thread might be locked in a wait
    if(closing_signal == S_FAST)
    {
        printf("Joining reader thread.\n");
        COND_SIGNAL(&clients_connected_cond);
        pthread_join(thread_reader_id, NULL);
    }

    return SERVER_OK;
}

int server_cleanup()
{
    printf("Freeing memory.\n");

    icl_hash_destroy(files_stored, free_keys_ht, free_data_ht);
    free(thread_workers_ids);

    LOCK_MUTEX(&clients_list_mutex);
    ll_empty(clients_connected, NULL);
    UNLOCK_MUTEX(&clients_list_mutex);

    printf("Closing socket and removing it.\n");
    close(server_socket_id);

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(loaded_configuration, socket_name);
    remove(socket_name);

    print_logging(server_log);
    free_log(server_log);

    // this function returns only SERVER_OK but can be expanded with other return values
    return SERVER_OK;
}

int start_server()
{
    int lastest_status;

    files_stored = icl_hash_create(10, NULL, NULL);
    server_log = create_log();
    initialize_policy_delegate();

    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_accepter, lastest_status);
    // Initialize and run reader
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_reader, lastest_status);

    printf("Server started with PID:%d.\n", getpid());

    // blocking call, return on quit signal
    server_wait_end_signal();
    server_wait_for_threads();

    return server_cleanup();
}

int init_server(const configuration_params_t* config)
{
    loaded_configuration = (configuration_params_t*)config;

    server_socket_id = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK_ERROR_EQ(server_socket_id, socket(AF_UNIX, SOCK_STREAM, 0), -1, ERR_SOCKET_FAILED, "Couldn't initialize socket!");

    char socket_name[MAX_PATHNAME_API_LENGTH];
    config_get_socket_name(loaded_configuration, socket_name);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, socket_name, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    remove(socket_name);

    CHECK_ERROR_EQ(server_socket_id, bind(server_socket_id, (struct sockaddr*)&sa,
                     sizeof(sa)), -1, ERR_SOCKET_BIND_FAILED, "Couldn't bind socket!");
    CHECK_ERROR_EQ(server_socket_id, listen(server_socket_id, 
                    config_get_backlog_sockets_num(loaded_configuration)), -1, ERR_SOCKET_LISTEN_FAILED, "Couldn't listen socket!");

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