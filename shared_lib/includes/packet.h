#ifndef _PACKET_H_
#define _PACKET_H_

#include <stdlib.h>
#include <stdint.h>
#include "network_file.h"

typedef uint32_t packet_op;
typedef uint32_t packet_len;

typedef struct packet_header packet_header_t;
typedef struct packet packet_t;

packet_t* create_packet(packet_op op, ssize_t initial_capacity);
void clear_packet(packet_t* p);
void destroy_packet(packet_t* p);

packet_t* read_packet_from_fd(int fd);
int send_packet_to_fd(int id, packet_t* p);

int write_data(packet_t* p, const void* data, size_t size);
int write_data_str(packet_t* p, const char* str, size_t len);
int read_data(packet_t* p, void* buf, size_t size);
int read_data_str_alloc(packet_t* p, char** str);
int read_data_str(packet_t* p, char* str, size_t input_str_length);

// getters and setters
int packet_get_remaining_byte_count(packet_t* p);

packet_op packet_get_op(packet_t* p);
int packet_get_sender(packet_t* p);
packet_len packet_get_length(packet_t* p);

void packet_set_op(packet_t* p, packet_op op);
void packet_set_sender(packet_t* p, int sender);

int is_packet_valid(packet_t* p);
void print_packet(packet_t* p);

/* others */
int write_netfile(packet_t* p, network_file_t* file);
int read_netfile(packet_t* p, network_file_t** output);

#endif