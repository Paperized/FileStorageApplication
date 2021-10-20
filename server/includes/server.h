#ifndef _SERVER_
#define _SERVER_

// Define used to add a custom name to the logs (utils.h)
#undef APP_NAME
#define APP_NAME "Server"

#include <pthread.h>

#include "config_params.h"
#include "queue.h"
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
#define SERVER_OK 0

typedef struct server server_t;

// Get the quit signal for the server
quit_signal_t get_quit_signal();

// Get the global file system for the server
file_system_t* get_fs();

// Get the global logger for the server
// Use LOG_EVENT to log a formatted string
logging_t* get_log();

// Initialize the server by allocating the needed memory, binds the socket server and listens to it.
// *Needs a configutation in input to work.*
int init_server(const configuration_params_t* config);

// Starts the server by starting the workers and the connection handler then listen to signals.
int start_server();

// Used to log events in the server log file
// If length is less the MAX_LOG_LINE_LENGTH the default synchronized buffer is used, otherwise a new string is allocated with that length
#define LOG_EVENT(str, length, ...) if(length == -1 || length < MAX_LOG_LINE_LENGTH) { \
                                        LOG_FORMATTED_LINE(get_log(), str, ## __VA_ARGS__); \
                                    } else { \
                                        LOG_FORMATTED_N_LINE(get_log(), length, str, ## __VA_ARGS__); \
                                    }

#endif