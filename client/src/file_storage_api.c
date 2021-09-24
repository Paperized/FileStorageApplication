#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

#include "file_storage_api.h"
#include "server_api_utils.h"
#include "client_params.h"
#include "packet.h"

#define CLEANUP_PACKETS(sent, received) destroy_packet(sent); \
                                        destroy_packet(received)

#define CHECK_WRITE_PACKET(pk, write_res) if(write_res == -1) { \
                                                destroy_packet(pk); \
                                                PRINT_WARNING(errno, "Cannot write data inside packet!", NO_ARGS); \
                                                return -1; \
                                            }

#define CHECK_READ_PACKET(pk, read_res, req) if(read_res == -1) { \
                                                CLEANUP_PACKETS(pk, req); \
                                                PRINT_WARNING(errno, "Cannot read data inside packet!", NO_ARGS); \
                                                return -1; \
                                            }

#define WRITE_PACKET_STR(pk, write_res, str, len) write_res = write_data_str(pk, str, len); \
                                            CHECK_WRITE_PACKET(pk, write_res)

#define WRITE_PACKET(pk, write_res, data_ptr, size) write_res = write_data(pk, data_ptr, size); \
                                            CHECK_WRITE_PACKET(pk, write_res)

#define READ_PACKET_STR(pk, read_res, str, len, req) read_res = read_data_str(pk, data_ptr, len); \
                                            CHECK_READ_PACKET(pk, read_res, req)
                                        
#define READ_PACKET(pk, read_res, data_ptr, size, req) read_res = read_data(pk, data_ptr, size); \
                                            CHECK_READ_PACKET(pk, read_res, req)

#define READ_FILE_PACKET(pk, read_res, file_dptr, req) read_res = read_netfile(pk, file_dptr); \
                                            CHECK_READ_PACKET(pk, read_res, req)

#define RET_ON_ERROR(req, res) if(packet_get_op(res) == OP_ERROR) \
                                { \
                                    int read_res; \
                                    server_open_file_options_t err; \
                                    READ_PACKET(res, read_res, &err, sizeof(server_open_file_options_t), req); \
                                    errno = err; \
                                    CLEANUP_PACKETS(req, res); \
                                    return -1; \
                                }

#define DEBUG_OK(req) if(packet_get_op(res) == OP_OK) \
                        printf("OK.\n");

#define SEND_TO_SERVER(req, error) error = send_packet_to_fd(fd_server, req); \
                                    if(error == -1) \
                                    { \
                                        destroy_packet(req); \
                                        PRINT_WARNING(error, "Cannot send packet to server!", NO_ARGS); \
                                        return -1; \
                                    }

#define WAIT_UNTIL_RESPONSE(res, req, error) res = wait_response_from_server(&error); \
                                        if(error == -1) \
                                        { \
                                            destroy_packet(req); \
                                            PRINT_WARNING(error, "Cannot receive packet from server!", NO_ARGS); \
                                            return -1; \
                                        }

int fd_server;

// NULL -> error
packet_t* wait_response_from_server(int* error)
{
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(fd_server, &fdset);

    int res = select(fd_server + 1, &fdset, NULL, NULL, NULL);
    if(res == -1)
    {
        error = 0;
        return NULL;
    }

    return read_packet_from_fd(fd_server);
}

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
    int remaining_ms = (abstime.tv_sec + abstime.tv_nsec / 1000000000) - time(0);
    CHECK_ERROR_EQ(fd_server, socket(AF_UNIX, SOCK_STREAM, 0), -1, -1, "Cannot connect to server!");

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    int result_socket;
    while(remaining_ms > 0 && (result_socket = connect(fd_server, (struct sockaddr*)&sa, sizeof(sa))) == -1)
    {
        remaining_ms -= msec;
        sleep(msec);
    }

    return result_socket;
}

int closeConnection(const char* sockname)
{
    packet_t* of_packet = create_packet(OP_CLOSE_CONN, 0);

    int error;
    SEND_TO_SERVER(of_packet, error);
    
    return close(fd_server);
}

int openFile(const char* pathname, int flags)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* of_packet = create_packet(OP_OPEN_FILE, sizeof(int) + path_size);
    int error;
    WRITE_PACKET(of_packet, error, &flags, sizeof(int));
    WRITE_PACKET_STR(of_packet, error, pathname, path_size);

    SEND_TO_SERVER(of_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, of_packet, error);

    RET_ON_ERROR(of_packet, res);
    DEBUG_OK(res);

    // leggo la risposta
    CLEANUP_PACKETS(of_packet, res);
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_READ_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);
    
    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    int buffer_size = packet_get_remaining_byte_count(res);
    CHECK_FATAL_ERRNO(*buf, malloc(buffer_size), NO_MEM_FATAL);
    READ_PACKET(res, error, *buf, buffer_size, rf_packet);
    *size = buffer_size;


    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int readNFiles(int N, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_READN_FILES, sizeof(int));
    int error;
    WRITE_PACKET(rf_packet, error, &N, sizeof(int));
    
    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    int num_read;
    READ_PACKET(res, error, &num_read, sizeof(int), rf_packet);
    char full_path[MAX_PATHNAME_API_LENGTH + 1];

    for(int i = 0; i < num_read; ++i)
    {
        network_file_t* file_received;
        READ_FILE_PACKET(res, error, &file_received, rf_packet);
        // salva su disco
        char* pathname = netfile_get_pathname(file_received);
        void* data = netfile_get_data(file_received);
        size_t data_size = netfile_get_data_size(file_received);

        if(pathname)
        {
            // save file
            size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
            size_t filename_len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
            if(buildpath(full_path, (char*)dirname, pathname, dirname_len, filename_len) == -1)
            {
                PRINT_ERROR(errno, "ReadN (Saving) %s exceeded max path length (%zu)!", pathname, dirname_len + filename_len + 1);
            }
            else
            {
                if(write_file_util(full_path, data, data_size) == -1)
                {
                    PRINT_ERROR(errno, "ReadN (Saving) %s failed!", pathname);
                }
            }
        }
        
        free(file_received);
    }

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int writeFile(const char* pathname, const char* dirname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_WRITE_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);
    bool_t receive_back_files = dirname != NULL;
    WRITE_PACKET(rf_packet, error, &receive_back_files, sizeof(bool_t));

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    if(receive_back_files)
    {
        int num_read;
        READ_PACKET(res, error, &num_read, sizeof(int), rf_packet);
        char full_path[MAX_PATHNAME_API_LENGTH + 1];

        for(int i = 0; i < num_read; ++i)
        {
            network_file_t* file_received;
            READ_FILE_PACKET(res, error, &file_received, rf_packet);
            // salva su disco
            char* pathname_recv = netfile_get_pathname(file_received);
            void* data = netfile_get_data(file_received);
            size_t data_size = netfile_get_data_size(file_received);

            if(pathname_recv)
            {
                // save file
                size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
                size_t filename_len = strnlen(pathname_recv, MAX_PATHNAME_API_LENGTH);
                if(buildpath(full_path, (char*)dirname, pathname_recv, dirname_len, filename_len) == -1)
                {
                    PRINT_ERROR(errno, "Write file (Replaced Files) %s exceeded max path length (%zu)!", pathname_recv, dirname_len + filename_len + 1);
                }
                else
                {
                    if(write_file_util(full_path, data, data_size) == -1)
                    {
                        PRINT_ERROR(errno, "Write file (Replaced Files) %s failed!", pathname_recv);
                    }
                }
            }

            free_netfile(file_received);
        }
    }

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_APPEND_FILE, size + path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);
    bool_t receive_back_files = dirname != NULL;
    WRITE_PACKET(rf_packet, error, &receive_back_files, sizeof(bool_t));
    WRITE_PACKET(rf_packet, error, buf, size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    if(receive_back_files)
    {
        int num_read;
        READ_PACKET(res, error, &num_read, sizeof(int), rf_packet);
        char full_path[MAX_PATHNAME_API_LENGTH + 1];
        
        for(int i = 0; i < num_read; ++i)
        {
            network_file_t* file_received;
            READ_FILE_PACKET(res, error, &file_received, rf_packet);
            // salva su disco
            char* pathname_recv = netfile_get_pathname(file_received);
            void* data = netfile_get_data(file_received);
            size_t data_size = netfile_get_data_size(file_received);

            if(pathname_recv)
            {
                // save file
                size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
                size_t filename_len = strnlen(pathname_recv, MAX_PATHNAME_API_LENGTH);
                if(buildpath(full_path, (char*)dirname, pathname_recv, dirname_len, filename_len) == -1)
                {
                    PRINT_ERROR(errno, "Write file (Replaced Files) %s exceeded max path length (%zu)!", pathname_recv, dirname_len + filename_len + 1);
                }
                else
                {
                    if(write_file_util(full_path, data, data_size) == -1)
                    {
                        PRINT_ERROR(errno, "Write file (Replaced Files) %s failed!", pathname_recv);
                    }
                }
            }

            free_netfile(file_received);
        }
    }

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int closeFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_CLOSE_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int removeFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_REMOVE_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int lockFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_LOCK_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int unlockFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    packet_t* rf_packet = create_packet(OP_UNLOCK_FILE, path_size);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}