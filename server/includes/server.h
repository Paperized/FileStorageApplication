#ifndef _SERVER_
#define _SERVER_

#include <pthread.h>
#include "packet.h"
#include "config_params.h"
#include "queue.h"

#define TRUE 1
#define FALSE 0
typedef int bool_t;

#define S_NONE 0
#define S_SOFT 1
#define S_FAST 2
typedef int quit_signal_t;

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

extern pthread_cond_t request_received_cond;

extern pthread_mutex_t clients_list_mutex;
extern linked_list_t clients_connected;

extern pthread_mutex_t clients_set_mutex;
extern fd_set clients_connected_set;

extern pthread_mutex_t requests_queue_mutex;
extern queue_t requests_queue;

extern pthread_mutex_t quit_signal_mutex;
extern quit_signal_t quit_signal;

extern configuration_params loaded_configuration;
extern int server_socket_id;

quit_signal_t get_quit_signal();
void set_quit_signal(quit_signal_t value);
int load_config_server();
int init_server();
int start_server();

#endif