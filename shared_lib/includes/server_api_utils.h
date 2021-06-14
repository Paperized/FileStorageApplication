#ifndef _SERVER_API_UTILS_H_
#define _SERVER_API_UTILS_H_

#include <errno.h>

typedef enum server_packet_op {
    OP_UNKNOWN,
    OP_OPEN_FILE,
    OP_READ_FILE,
    OP_READN_FILES,
    OP_WRITE_FILE,
    OP_APPEND_FILE,
    OP_CLOSE_FILE,
    OP_REMOVE_FILE,
    OP_CLOSE_CONN,
    OP_ERROR,
    OP_OK
} server_packet_op_t;

typedef enum server_open_file_options {
    OP_CREATE = 1,
    OP_LOCK = 2
} server_open_file_options_t;

typedef enum server_errors {
    ERR_NONE = 0,
    ERR_FILE_ALREADY_EXISTS = EEXIST,
    ERR_PATH_NOT_EXISTS = ENOENT,
    ERR_FILE_NOT_OPEN = EACCES
} server_errors_t;

const char* get_error_str(server_errors_t err);

#define MAX_PATHNAME_API_LENGTH 108

#endif