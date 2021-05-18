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

typedef enum server_open_file_options {
    OP_CREATE = 0x1,
    OP_LOCK = 0x2
} server_open_file_options_t;

#define MAX_PATHNAME_API_LENGTH 108

#endif