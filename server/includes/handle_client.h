#ifndef _HANDLE_CLIENT_H_
#define _HANDLE_CLIENT_H_

#include "packet.h"
#include "server_api_utils.h"

int handle_open_file_req(packet_t* req, pthread_t curr);
int handle_write_file_req(packet_t* req, pthread_t curr);
int handle_append_file_req(packet_t* req, pthread_t curr);
int handle_read_file_req(packet_t* req, pthread_t curr);
int handle_nread_files_req(packet_t* req, pthread_t curr);
int handle_remove_file_req(packet_t* req, pthread_t curr);
int handle_close_file_req(packet_t* req, pthread_t curr);

#endif