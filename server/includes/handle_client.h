#ifndef _HANDLE_CLIENT_H_
#define _HANDLE_CLIENT_H_

#include "server_api_utils.h"

int handle_open_file_req(int sender);
int handle_write_file_req(int sender);
int handle_append_file_req(int sender);
int handle_read_file_req(int sender);
int handle_nread_files_req(int sender);
int handle_remove_file_req(int sender);
int handle_lock_file_req(int sender);
int handle_unlock_file_req(int sender);
int handle_close_file_req(int sender);

#endif