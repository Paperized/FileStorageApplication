#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdint.h>

#define OP_OPEN_FILE 0
#define OP_READ_FILE 1
#define OP_WRITE_FILE 2
#define OP_APPEND_FILE 3
#define OP_CLOSE_FILE 4
#define OP_REMOVE_FILE 5
#define OP_CLOSE_CONN 6
#define OP_UNKNOWN 7

#define F_CREATE 0

#define PACKET_EMPTY 0

typedef uint32_t packet_op;
typedef uint32_t packet_len;

typedef struct packet_header_t {
    packet_op op;
    int fd_sender;
    packet_len len;
} packet_header_t;

typedef struct packet_body_t {
    char* content;
    packet_len cursor_index;
} packet_body_t;

typedef struct packet_t {
    packet_header_t header;
    packet_body_t body;
} packet_t;

#define INIT_EMPTY_PACKET(op) { { op, -1, 0 }, { (void*)0, 0 } }

packet_t* read_packet_from_fd(int fd);
int send_packet_to_fd(int id, packet_t* p);
void destroy_packet(packet_t* p);

int is_packet_valid(packet_t* p);

#endif