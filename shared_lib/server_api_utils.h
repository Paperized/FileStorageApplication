#ifndef _SERVER_API_UTILS_H_
#define _SERVER_API_UTILS_H_

typedef enum server_packet_op {
    OP_UNKNOWN,
    OP_OPEN_FILE,
    OP_READ_FILE,
    OP_WRITE_FILE,
    OP_APPEND_FILE,
    OP_CLOSE_FILE,
    OP_REMOVE_FILE,
    OP_CLOSE_CONN
} server_packet_op_t;

#define CONFIG_MAX_SOCKET_NAME_LENGTH 64

#endif