#ifndef _HANDLE_CLIENT_H_
#define _HANDLE_CLIENT_H_

#include "packet.h"
#include "server_api_utils.h"

int handle_open_file_req(packet_t* req, packet_t* response);
int handle_write_file_req(packet_t* req, packet_t* response);
int handle_append_file_req(packet_t* req, packet_t* response);
int handle_read_file_req(packet_t* req, packet_t* response);
int handle_nread_files_req(packet_t* req, packet_t* response);
int handle_remove_file_req(packet_t* req, packet_t* response);
int handle_lock_file_req(packet_t* req, packet_t* response);
int handle_unlock_file_req(packet_t* req, packet_t* response);
int handle_close_file_req(packet_t* req, packet_t* response);

extern packet_t* p_on_file_deleted_locks;
extern packet_t* p_on_file_given_lock;

#endif