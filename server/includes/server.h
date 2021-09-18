#ifndef _SERVER_
#define _SERVER_

#include <pthread.h>
#include "packet.h"
#include "config_params.h"
#include "queue.h"
#include "utils.h"
#include "icl_hash.h"
#include "logging.h"

typedef enum quit_signal {
    S_NONE,
    S_SOFT,
    S_FAST
} quit_signal_t;

#define ERR_SOCKET_FAILED -1
#define ERR_SOCKET_BIND_FAILED -2
#define ERR_SOCKET_LISTEN_FAILED -3
#define ERR_SOCKET_OUT_OF_MEMORY -4
#define ERR_SOCKET_INIT_WORKERS -5
#define ERR_SERVER_SIGNALS -6
#define ERR_SOCKET_INIT_ACCEPTER -7
#define ERR_SERVER_INIT_READER -8
#define SERVER_OK 0

// GESTIRE SIGINT, SIGQUIT (Chiusura il prima possibile, non accetta nuove richieste e chiude) 
// e SIGHUP non accetta nuove richieste e finisce con quelle rimanenti

typedef struct file_stored {
    char* data;
    size_t size;
    int locked_by;
    linked_list_t* opened_by;
    queue_t* lock_queue;
    struct timespec creation_time;
    struct timespec last_use_time;
    bool_t can_be_removed;
    uint32_t use_frequency;
    pthread_mutex_t rw_mutex;
} file_stored_t;

extern pthread_mutex_t files_stored_mutex;
extern icl_hash_t* files_stored;

extern pthread_mutex_t current_used_memory_mutex;
extern size_t current_used_memory;

extern pthread_cond_t clients_connected_cond;
extern pthread_mutex_t clients_list_mutex;
extern linked_list_t* clients_connected;

extern pthread_mutex_t clients_set_mutex;
extern fd_set clients_connected_set;

extern pthread_cond_t request_received_cond;
extern pthread_mutex_t requests_queue_mutex;
extern queue_t* requests_queue;

extern pthread_mutex_t quit_signal_mutex;
extern quit_signal_t quit_signal;

extern pthread_mutex_t loaded_configuration_mutex;
extern configuration_params_t* loaded_configuration;
extern int server_socket_id;

extern pthread_mutex_t server_log_mutex;
extern logging_t* server_log;

extern int (*server_policy)(file_stored_t* f1, file_stored_t* f2);

void free_keys_ht(void* key);
void free_data_ht(void* key);

quit_signal_t get_quit_signal();
void set_quit_signal(quit_signal_t value);
int init_server(const configuration_params_t* config);
int start_server();

#endif