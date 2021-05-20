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
                                        destroy_packet(received);

#define CHECK_WRITE_PACKET(res) if(res == -1) { \
                                    printf("You cannot write in a NULL packet.\n"); \
                                    return -1; \
                                } \
                                else if(res == -2) { \
                                    printf("Malloc or realloc failed during write packet. No memory available.\n"); \
                                    return -1; \
                                }

#define CHECK_READ_PACKET(res) if(res == -1) { \
                                    printf("You cannot read in a NULL packet.\n"); \
                                    return -1; \
                                } \
                                else if(res == -2) { \
                                    printf("Malloc or realloc failed during read packet. No memory available.\n"); \
                                    return -1; \
                                } \
                                else if(res == -3) { \
                                    printf("Your cursor reached out of bound in the packet during a read.\n"); \
                                    return -1; \
                                }

#define RET_ON_ERROR(req, res, error) if(res->header.op == OP_ERROR) \
                                { \
                                    server_open_file_options_t* err = read_data(res, sizeof(server_open_file_options_t), &error); \
                                    CHECK_READ_PACKET(error); \
                                    errno = *err; \
                                    printf("Operation ended with error: %s.\n", get_error_str(*err)); \
                                    free(err); \
                                    CLEANUP_PACKETS(req, res); \
                                    WAIT_TIMER; \
                                    return -1; \
                                }

#define DEBUG_OK(req) if(req->header.op == OP_OK) \
                    printf("OK.\n");

#define WAIT_TIMER if(g_params.ms_between_requests > 0) \
                    msleep(g_params.ms_between_requests)

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

    return read_packet_from_fd(fd_server, error);
}

void timespec_to_ns(const struct timespec abstime)
{
    int tot = 0;
    tot = abstime.tv_sec * 1000;
    tot += abstime.tv_nsec / 1000000;

    return tot;
}

void msleep(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    msleep_ts(ts);
}

void msleep_ts(const struct timespec abstime)
{
    nanosleep(&abstime, NULL);
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
    packet_t* of_packet = create_packet(OP_CLOSE_CONN);

    int p_result = send_packet_to_fd(fd_server, of_packet);
    if(p_result == -1)
    {
        // set errno
        return -1;
    }

    /*
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(&of_packet);
        return -1;
    }

    if(response->header.op != OP_OPEN_FILE)
    {
        CLEANUP_PACKETS(&of_packet, response);
        return -1;
    }

    // leggo la risposta
    CLEANUP_PACKETS(&of_packet, response);*/

    int result = close(fd_server);
    return result;
}

int openFile(const char* pathname, int flags)
{
    packet_t* of_packet = create_packet(OP_OPEN_FILE);
    int error;
    error = write_data(of_packet, &flags, sizeof(int));
    CHECK_WRITE_PACKET(error);
    error = write_data(of_packet, pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH) * sizeof(char));
    CHECK_WRITE_PACKET(error);

    print_packet(of_packet);
    int result = send_packet_to_fd(fd_server, of_packet);
    if(result == -1)
    {
        // set errno
        printf("Failed sending packet.\n");
        destroy_packet(of_packet);
        return -1;
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        printf("Failed receiving packet.\n");
        destroy_packet(of_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(of_packet, response, error);
    DEBUG_OK(response);

    // leggo la risposta
    CLEANUP_PACKETS(of_packet, response);
    WAIT_TIMER;
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
    packet_t* rf_packet = create_packet(OP_READ_FILE);
    int error;
    error = write_data(rf_packet, pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH) * sizeof(char));
    CHECK_WRITE_PACKET(error);
    
    int result = send_packet_to_fd(fd_server, rf_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(rf_packet, response, error);

    char* file_readed = read_data(response, response->header.len, &error);
    if(error < 0)
    {

        CLEANUP_PACKETS(rf_packet, response);
        WAIT_TIMER;
        return -1;
    }

    *buf = file_readed;
    *size = response->header.len;
    CLEANUP_PACKETS(rf_packet, response);
    WAIT_TIMER;
    return 0;
}

int writeFile(const char* pathname, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_WRITE_FILE);
    int error;
    error = write_data(rf_packet, pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH) * sizeof(char));
    CHECK_WRITE_PACKET(error);

    int result = send_packet_to_fd(fd_server, rf_packet);
    if(result == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        return -1;
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(rf_packet, response, error);
    DEBUG_OK(response);

    CLEANUP_PACKETS(rf_packet, response);
    WAIT_TIMER;
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
    packet_t* rf_packet = create_packet(OP_APPEND_FILE);
    int error;
    error = write_data(rf_packet, buf, size);
    CHECK_WRITE_PACKET(error);

    int result = send_packet_to_fd(fd_server, rf_packet);
    if(result == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        return -1;
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(rf_packet, response, error);
    DEBUG_OK(response);

    CLEANUP_PACKETS(rf_packet, response);
    WAIT_TIMER;
    return 0;
}

int closeFile(const char* pathname)
{
    packet_t* rf_packet = create_packet(OP_CLOSE_FILE);
    int error;
    error = write_data(rf_packet, pathname, strnlen(pathname, 100) * sizeof(char)); // set a constant later 
    CHECK_WRITE_PACKET(error);

    int result = send_packet_to_fd(fd_server, rf_packet);
    if(result == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        return -1;
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(rf_packet, response, error);
    DEBUG_OK(response);

    CLEANUP_PACKETS(rf_packet, response);
    WAIT_TIMER;
    return 0;
}

int removeFile(const char* pathname)
{
    packet_t* rf_packet = create_packet(OP_REMOVE_FILE);
    int error;
    error = write_data(rf_packet, pathname, strnlen(pathname, 100) * sizeof(char)); // set a constant later 
    CHECK_WRITE_PACKET(error);

    int result = send_packet_to_fd(fd_server, rf_packet);
    if(result == -1)
    {
        // set errno
        return -1;
        destroy_packet(rf_packet);
    }

    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        destroy_packet(rf_packet);
        WAIT_TIMER;
        return -1;
    }

    RET_ON_ERROR(rf_packet, response, error);
    DEBUG_OK(response);

    CLEANUP_PACKETS(rf_packet, response);
    WAIT_TIMER;
    return 0;
}