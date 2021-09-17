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

#define RET_ON_ERROR(req, res) if(packet_get_op(res) == OP_ERROR) \
                                { \
                                    int read_res; \
                                    server_open_file_options_t err; \
                                    READ_PACKET(res, read_res, &err, sizeof(server_open_file_options_t), req); \
                                    CLEANUP_PACKETS(req, res); \
                                    return -1; \
                                }

#define DEBUG_OK(req) if(packet_get_op(res) == OP_OK) \
                        printf("OK.\n");

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

#define WAIT_TIMER {}

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
    int available_time = abstime.tv_sec;
    fd_server = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    int result_socket;
    while(available_time > 0 && (result_socket = connect(fd_server, (struct sockaddr*)&sa, sizeof(sa))) != 0)
    {
        sleep(msec);
        available_time -= msec;
    }

    if(result_socket == -1)
    {
        // set errno

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
    packet_t* of_packet = create_packet(OP_OPEN_FILE, sizeof(int) + MAX_PATHNAME_API_LENGTH);
    int error;
    WRITE_PACKET(of_packet, error, &flags, sizeof(int));
    WRITE_PACKET_STR(of_packet, error, pathname, MAX_PATHNAME_API_LENGTH);
    
    print_packet(of_packet);

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
    packet_t* rf_packet = create_packet(OP_READ_FILE, MAX_PATHNAME_API_LENGTH);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);
    
    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    int buffer_size = packet_get_remaining_byte_count(res);
    CHECK_FATAL_ERRNO(*buf, malloc(buffer_size), NO_MEM_FATAL);
    READ_PACKET(res, error, *buf, buffer_size, rf_packet);

    // salva su disco

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int readNFiles(int N, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_READN_FILES, sizeof(int) + MAX_PATHNAME_API_LENGTH);
    int error;
    WRITE_PACKET(rf_packet, error, &N, sizeof(int));
    WRITE_PACKET(rf_packet, error, dirname, MAX_PATHNAME_API_LENGTH);
    
    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);
    RET_ON_ERROR(rf_packet, res);

    // salva su disco / leggi risposta

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int writeFile(const char* pathname, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_WRITE_FILE, MAX_PATHNAME_API_LENGTH * 2);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);
    WRITE_PACKET_STR(rf_packet, error, dirname, MAX_PATHNAME_API_LENGTH);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    // salva eventualmente il file espulso su disco

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_APPEND_FILE, size + MAX_PATHNAME_API_LENGTH * 2);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);
    WRITE_PACKET_STR(rf_packet, error, dirname, MAX_PATHNAME_API_LENGTH);
    WRITE_PACKET(rf_packet, error, buf, size);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    // salva eventualmente i file esplulsi dal server

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int closeFile(const char* pathname)
{
    packet_t* rf_packet = create_packet(OP_CLOSE_FILE, MAX_PATHNAME_API_LENGTH);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    // controlla la risposta

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}

int removeFile(const char* pathname)
{
    packet_t* rf_packet = create_packet(OP_REMOVE_FILE, MAX_PATHNAME_API_LENGTH);
    int error;
    WRITE_PACKET_STR(rf_packet, error, pathname, MAX_PATHNAME_API_LENGTH);

    SEND_TO_SERVER(rf_packet, error);

    packet_t* res;
    WAIT_UNTIL_RESPONSE(res, rf_packet, error);

    RET_ON_ERROR(rf_packet, res);
    DEBUG_OK(res);

    // controllo risposta

    CLEANUP_PACKETS(rf_packet, res);
    return 0;
}