#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdlib.h>
#include <stdint.h>

#define F_CREATE 0
#define PACKET_EMPTY 0

typedef uint32_t packet_op;
typedef uint32_t packet_len;

typedef struct packet_header_t {
    packet_op op;
    int fd_sender;
    packet_len len;
} packet_header_t;

typedef struct packet_t {
    packet_header_t header;
    char* content;
    packet_len capacity;
    packet_len cursor_index;
} packet_t;

packet_t* create_packet(packet_op op, ssize_t initial_capacity);
packet_t* read_packet_from_fd(int fd);
int send_packet_to_fd(int id, packet_t* p);
int write_data(packet_t* p, const void* data, size_t size);
int write_data_str(packet_t* p, const char* str);
int read_data(packet_t* p, void* buf, size_t size);
int read_data_str_alloc(packet_t* p, char** str);
int read_data_str(packet_t* p, char* str, size_t input_str_length);
void destroy_packet(packet_t* p);

int is_packet_valid(packet_t* p);
void print_packet(packet_t* p);

#endif