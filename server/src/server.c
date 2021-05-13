#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "server.h"

#define INITIALIZE_SERVER_FUNCTIONALITY(initializer, status) status = initializer(); \
                                                                if(status != SERVER_OK) \
                                                                { \
                                                                    rollback_server_and_quit(); \
                                                                    return status; \
                                                                }

#define CHECK_ERROR(boolean, returned_error) if(boolean) return returned_error

#define SKIP_WRONG_ACCEPT_RET(accept_value, output) if(accept_value == -1) continue; \
                                                output = accept_value

#define BREAK_ON_FAST_QUIT(qv, m)  if((qv = get_quit_signal()) == S_FAST) \
                                    { \
                                        pthread_mutex_unlock(m); \
                                        break; \
                                    }


configuration_params loaded_configuration;
int server_socket_id;

pthread_t* thread_workers_ids;
pthread_t thread_signals_id;
pthread_t thread_accepter_id;

pthread_mutex_t clients_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_received_cond = PTHREAD_COND_INITIALIZER;
queue_t clients_queue = INIT_EMPTY_QUEUE;

pthread_mutex_t quit_signal_mutex = PTHREAD_MUTEX_INITIALIZER;
quit_signal_t quit_signal = S_NONE;

/** VARIABLE STATUS **/
bool_t socket_initialized = FALSE;

bool_t workers_initialized = FALSE;
unsigned int workers_count = 0;

bool_t signals_initialized = FALSE;

bool_t connection_accepter_initialized = FALSE;

int load_config_server()
{
    CHECK_ERROR(load_configuration_params(&loaded_configuration) == -1, ERR_READING_CONFIG);
    return SERVER_OK;
}

quit_signal_t get_quit_signal()
{
    quit_signal_t result;

    pthread_mutex_lock(&quit_signal_mutex);
    result = quit_signal;
    pthread_mutex_unlock(&quit_signal_mutex);

    return result;
}

void set_quit_signal(quit_signal_t value)
{
    pthread_mutex_lock(&quit_signal_mutex);
    quit_signal = value;
    pthread_mutex_unlock(&quit_signal_mutex);
}

void* handle_client_requests(void* data)
{
    printf("Running worker thread\n");
    quit_signal_t quit_signal;
    while((quit_signal = get_quit_signal()) == S_NONE)
    {
        int client_handled;
        pthread_mutex_lock(&clients_queue_mutex);
        while(count(&clients_queue) == 0)
        {
            if(pthread_cond_wait(&client_received_cond, &clients_queue_mutex) != 0)
            {
                pthread_mutex_unlock(&clients_queue_mutex);
                continue;
            }

            BREAK_ON_FAST_QUIT(quit_signal, &clients_queue_mutex);
        }

        // quit worker loop if signal
        if(quit_signal == S_FAST)
            break;


        client_handled = dequeue(&clients_queue);
        pthread_mutex_unlock(&clients_queue_mutex);

        printf("[W/%lu] Handling client with id: %d.\n", pthread_self(), client_handled);
        sleep(1);
        printf("[W/%lu] Finished handling.\n", pthread_self());
        // Handle the message

        // extract message
        // check header
        // elaborate

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
        set_quit_signal(local_quit_signal);
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
        int new_id;
        bool_t queue_failed = FALSE;

        printf("Waiting for new connections.\n");
        SKIP_WRONG_ACCEPT_RET(accept(server_socket_id, NULL, 0), new_id);

        // add to queue
        queue_failed = enqueue_safe(&clients_queue, new_id, &clients_queue_mutex) != 0;

        // if something go wrong with the queue we close the connection right away
        if(queue_failed)
        {
            printf("Error: some problems may occourred with malloc in enqueue.\n");
            close(new_id);
        }
    }

    return NULL;
}

int initialize_connection_accepter()
{
    CHECK_ERROR(pthread_create(&thread_accepter_id, NULL, &handle_connections, NULL), ERR_SOCKET_INIT_ACCEPTER);

    connection_accepter_initialized = TRUE;
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
    closing_signal = get_quit_signal();

    pthread_cancel(thread_accepter_id);
    pthread_join(thread_accepter_id, NULL);

    pthread_cond_broadcast(&client_received_cond);

    printf("Joining workers thread.\n");
    for(int i = 0; i < loaded_configuration.thread_workers; ++i)
    {
        if(closing_signal == S_FAST)
            pthread_cancel(thread_workers_ids[i]);

        pthread_join(thread_workers_ids[i], NULL);
    }

    printf("Freeing memory.\n");
    free(thread_workers_ids);

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

    // Initialize and run signals
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_signals, lastest_status);
    // Initialize and run workers
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_workers, lastest_status);
    // Initialize and run connections
    INITIALIZE_SERVER_FUNCTIONALITY(initialize_connection_accepter, lastest_status);

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
    strncpy(sa.sun_path, loaded_configuration.socket_name, CONFIG_MAX_SOCKET_NAME_LENGTH);
    sa.sun_family = AF_UNIX;

    CHECK_ERROR(bind(server_socket_id, (struct sockaddr*)&sa, sizeof(sa)) == -1, ERR_SOCKET_BIND_FAILED);
    CHECK_ERROR(listen(server_socket_id, loaded_configuration.backlog_sockets_num) == -1, ERR_SOCKET_LISTEN_FAILED);

    socket_initialized = TRUE;
    return SERVER_OK;
}