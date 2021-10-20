#ifndef _HANDLE_CLIENT_H_
#define _HANDLE_CLIENT_H_

#include "server_api_utils.h"

// Used to awake a client waiting for the lock to be given, send back the OP_OK
void notify_given_lock(int client);

// Handles the sender open request by accessing the file system and returning a status code
// This method fails if on O_CREATE the file already exists or viceversa
// Returns -1 if the flag O_LOCK was sent but the lock is currently owned by another client
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_open_file_req(int sender);

// Handles the sender write request by accessing the file system and returning a status code
// This method fails if the file doesn't exist, if the previous sender request on this file was not an open with flags O_CREATE | O_LOCK or the data is too big
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_write_file_req(int sender);

// Handles the sender append request by accessing the file system and returning a status code
// This method fails if the file doesn't exist, if the file is not opened by the sender, if the file is owned by another client or the data to be appended is too big
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_append_file_req(int sender);

// Handles the sender read request by accessing the file system and returning a status code
// This method fails if the file doesn't exist, if the file is not opened by the sender or if the file is owned by another client
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_read_file_req(int sender);

// Handles the sender nread request by accessing the file system and returning a status code
// This method never fails
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_nread_files_req(int sender);

// Handles the sender remove request by accessing the file system and returning a status code
// This method fails if the file doesn't exist or if the file is not owned by the sender
// Send back an OP_ERROR to all clients waiting for the lock to be given
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_remove_file_req(int sender);

// Handles the sender open request by accessing the file system and returning a status code
// This method fails if the file doesn't exist or if the file is not opened by the sender
// Returns -1 if the file is already locked by another client
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_lock_file_req(int sender);

// Handles the sender unlock request by accessing the file system and returning a status code
// This method fails if the file doesn't exist, if the file is not opened by the sender or if the sender does not own the file
// On success this function awake the top client in the queue waiting for the lock
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_unlock_file_req(int sender);

// Handles the sender close request by accessing the file system and returning a status code
// This method fails if the file doesn't exist, if the file is not opened by the sender
// On success, if the sender owns the lock it is released and given to the next client waiting for it
//
// The status code can be: 
// 0) if the operation was succesfull and an OP_OK was sent back to the client
// -1) if the operation was succesfull but the answer will be sent back to the client in the future
// >0) if the operation was not succesfull and an OP_ERROR is sent back to the client with the relative error
int handle_close_file_req(int sender);

#endif