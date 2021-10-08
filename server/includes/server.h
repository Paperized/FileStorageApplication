#ifndef _SERVER_
#define _SERVER_

#include <pthread.h>

#include "packet.h"
#include "config_params.h"
#include "queue.h"

#define DEBUG_LOG 0
#include "utils.h"
#include "logging.h"
#include "file_system.h"

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

typedef struct server server_t;

extern pthread_mutex_t server_log_mutex;
extern logging_t* server_log;

quit_signal_t get_quit_signal();
file_system_t* get_fs();
logging_t* get_log();
void set_quit_signal(quit_signal_t value);

int init_server(const configuration_params_t* config);
int start_server();

#define LOG_EVENT(str, length, ...) if(length == -1 || length < MAX_LOG_LINE_LENGTH) { \
                                        LOG_FORMATTED_LINE(get_log(), str, ## __VA_ARGS__); \
                                    } else { \
                                        LOG_FORMATTED_N_LINE(get_log(), length, str, ## __VA_ARGS__); \
                                    }

#endif