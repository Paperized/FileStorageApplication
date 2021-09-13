#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdlib.h>
#include <stdint.h>

#define F_CREATE 0
#define PACKET_EMPTY 0

typedef uint32_t packet_op;
typedef uint32_t packet_len;

typedef struct packet_header packet_header_t;
typedef struct packet packet_t;

packet_t* create_packet(packet_op op, ssize_t initial_capacity);
packet_t* read_packet_from_fd(int fd);
int send_packet_to_fd(int id, packet_t* p);
int write_data(packet_t* p, const void* data, size_t size);
int write_data_str(packet_t* p, const char* str, size_t len);
int read_data(packet_t* p, void* buf, size_t size);
int read_data_str_alloc(packet_t* p, char** str);
int read_data_str(packet_t* p, char* str, size_t input_str_length);
void destroy_packet(packet_t* p);

// getters and setters
int packet_get_remaining_byte_count(packet_t* p);

int is_packet_valid(packet_t* p);
void print_packet(packet_t* p);

#endif