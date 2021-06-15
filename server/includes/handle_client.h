#ifndef _HANDLE_CLIENT_H_
#define _HANDLE_CLIENT_H_

#include "packet.h"
#include "server_api_utils.h"

void handle_open_file_req(packet_t* req, pthread_t curr);
void handle_write_file_req(packet_t* req, pthread_t curr);
void handle_append_file_req(packet_t* req, pthread_t curr);
void handle_read_file_req(packet_t* req, pthread_t curr);
void handle_nread_files_req(packet_t* req, pthread_t curr);
void handle_remove_file_req(packet_t* req, pthread_t curr);
void handle_close_file_req(packet_t* req, pthread_t curr);

#endif