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
                                                PRINT_WARNING(errno, "Cannot write data inside packet!"); \
                                                return -1; \
                                            }

#define CHECK_READ_PACKET(pk, read_res, req) if(read_res == -1) { \
                                                CLEANUP_PACKETS(pk, req); \
                                                PRINT_WARNING(errno, "Cannot read data inside packet!"); \
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

#define RET_ON_ERROR(req, pathname, res) if(packet_get_op(res) == OP_ERROR) \
                                { \
                                    int read_res; \
                                    server_open_file_options_t err; \
                                    READ_PACKET(res, read_res, &err, sizeof(server_open_file_options_t), req); \
                                    errno = err; \
                                    if(g_params->print_operations) { \
                                        PRINT_INFO("%s on %s ended with failure! [%s]", __func__, pathname, strerror(err)); \
                                    } \
                                    CLEANUP_PACKETS(req, res); \
                                    return -1; \
                                }

#define SEND_TO_SERVER(req, error) error = send_packet_to_fd(fd_server, req); \
                                    if(error == -1) \
                                    { \
                                        destroy_packet(req); \
                                        PRINT_WARNING(error, "Cannot send packet to server!"); \
                                        return -1; \
                                    }

#define WAIT_UNTIL_RESPONSE(res, req, error) res = wait_response_from_server(&error); \
                                        if(error == -1) \
                                        { \
                                            destroy_packet(req); \
                                            PRINT_WARNING(error, "Cannot receive packet from server!"); \
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

static int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
    CHECK_ERROR_EQ(fd_server, socket(AF_UNIX, SOCK_STREAM, 0), -1, -1, "Cannot connect to server!");
    long remaining_msec = (abstime.tv_sec * 1000 + abstime.tv_nsec / 1000000000) - time(0) * 1000;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    int result_socket;
    while(remaining_msec > 0 && (result_socket = connect(fd_server, (struct sockaddr*)&sa, sizeof(sa))) == -1)
    {
        remaining_msec -= msec;
        if(msleep(msec) == -1)
        {
            return -1;
        }
    }

    errno = 0;
    return result_socket;
}

int closeConnection(const char* sockname)
{
    packet_t* of_packet = create_packet(OP_CLOSE_CONN, 0);

    int error;
    SEND_TO_SERVER(of_packet, error);

    destroy_packet(of_packet);
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
    RET_ON_ERROR(of_packet, pathname, res);

    if(g_params->print_operations)
    {
        PRINT_INFO("openFile on %s ended with success! [%s]", pathname, strerror(0));
    }

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
    RET_ON_ERROR(rf_packet, pathname, res);

    int buffer_size = packet_get_remaining_byte_count(res);
    CHECK_FATAL_ERRNO(*buf, malloc(buffer_size), NO_MEM_FATAL);
    READ_PACKET(res, error, *buf, buffer_size, rf_packet);
    *size = buffer_size;

    if(g_params->print_operations)
    {
        PRINT_INFO("readFile on %s ended with success, %d bytes read! [%s]", pathname, buffer_size, strerror(0));
    }

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
    RET_ON_ERROR(rf_packet, "-", res);

    int num_read;
    size_t data_size_read = 0;
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
        data_size_read += data_size;
        if(pathname)
        {
            // save file
            size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
            size_t filename_len = 0;
            char* filename = get_filename_from_path(pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH), &filename_len);
            if(buildpath(full_path, (char*)dirname, filename, dirname_len, filename_len) == -1)
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
        
        free_netfile(file_received);
    }

    if(g_params->print_operations)
    {
        PRINT_INFO("readNFiles ended with success! %d files readed for a total of %zu bytes! [%s]", num_read, data_size_read, strerror(0));
    }

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int writeFile(const char* pathname, const char* dirname)
{
    if(!pathname)
    {
        errno = EINVAL;
        return -1;
    }
    size_t path_len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);

    void* data;
    size_t data_size;
    read_file_util(pathname, &data, &data_size);

    packet_t* rf_packet = create_packet(OP_WRITE_FILE, path_len + data_size + sizeof(bool_t));
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, path_len);
    bool_t receive_back_files = dirname != NULL;
    WRITE_PACKET(rf_packet, error, &receive_back_files, sizeof(bool_t));
    
    if(data_size > 0)
    {
        WRITE_PACKET(rf_packet, error, data, data_size);
        free(data);
    }

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);
    RET_ON_ERROR(rf_packet, pathname, res);

    int num_read = 0;
    if(receive_back_files)
    {
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
                size_t filename_len = 0;
                pathname_recv = get_filename_from_path(pathname_recv, strnlen(pathname_recv, MAX_PATHNAME_API_LENGTH), &filename_len);
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

    if(g_params->print_operations)
    {
        PRINT_INFO("writeFile on %s ended with success! %zu bytes written and %d files replaced! [%s]", 
                                pathname, data_size, num_read, strerror(0));
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

    if(size > 0)
    {
        WRITE_PACKET(rf_packet, error, buf, size);
    }

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);
    RET_ON_ERROR(rf_packet, pathname, res);

    int num_read = 0;
    if(receive_back_files)
    {
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

    if(g_params->print_operations)
    {
        PRINT_INFO("appendFile on %s ended with success! %zu bytes written and %d files replaced! [%s]", 
                                pathname, size, num_read, strerror(0));
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
    RET_ON_ERROR(rf_packet, pathname, res);

    if(g_params->print_operations)
    {
        PRINT_INFO("closeFile on %s ended with success! [%s]", pathname, strerror(0));
    }

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
    RET_ON_ERROR(rf_packet, pathname, res);

    if(g_params->print_operations)
    {
        PRINT_INFO("removeFile on %s ended with success! [%s]", pathname, strerror(0));
    }

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
    RET_ON_ERROR(rf_packet, pathname, res);

    if(g_params->print_operations)
    {
        PRINT_INFO("lockFile on %s ended with success! [%s]", pathname, strerror(0));
    }

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
    RET_ON_ERROR(rf_packet, pathname, res);

    if(g_params->print_operations)
    {
        PRINT_INFO("unlockFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}