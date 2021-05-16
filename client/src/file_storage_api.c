#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

#include "file_storage_api.h"
#include "server_api_utils.h"
#include "packet.h"

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

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
    int available_time = 0;
    fd_server = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, 100);
    sa.sun_family = AF_UNIX;

    int result_socket;
    while(available_time > 0 && (result_socket = connect(fd_server, (struct sockaddr*)&sa, sizeof(sa))) != 0)
    {
        usleep(msec);
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
    int result = close(/*??????*/1);

    if(result == -1)
    {
        // set errno
    }

    return result;
}

int openFile(const char* pathname, int flags)
{
    packet_t of_packet = INIT_EMPTY_PACKET(OP_OPEN_FILE);
    // add integer

    int result = send_packet_to_fd(fd_server, &of_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }

    // leggo la risposta

    free(response);
    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
    packet_t rf_packet = INIT_EMPTY_PACKET(OP_READ_FILE);
    // add integer

    int result = send_packet_to_fd(fd_server, &rf_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }

    // leggo la risposta
    size = malloc(sizeof(size_t));
    *size = response->header.len;

    //estraggo la stringa e la setto a buf
    // buf = get_string(response);

    free(response);
    return 0;
}

int writeFile(const char* pathname, const char* dirname)
{
    packet_t rf_packet = INIT_EMPTY_PACKET(OP_WRITE_FILE);
    // add string

    int result = send_packet_to_fd(fd_server, &rf_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }

    // leggo la risposta
    // se la risposta è positiva continuo altrimenti -1

    free(response);
    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
    packet_t rf_packet = INIT_EMPTY_PACKET(OP_APPEND_FILE);
    // add buffer

    int result = send_packet_to_fd(fd_server, &rf_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }


    //estraggo la stringa e la setto a buf
    // buf = get_string(response);

    free(response);
    return 0;
}

int closeFile(const char* pathname)
{
    packet_t rf_packet = INIT_EMPTY_PACKET(OP_CLOSE_FILE);
    // add integer

    int result = send_packet_to_fd(fd_server, &rf_packet);
    if(result == -1)
    {
        // set errno

        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }

    // leggo la risposta


    free(response);
    return 0;
}

int removeFile(const char* pathname)
{
    packet_t rf_packet = INIT_EMPTY_PACKET(OP_REMOVE_FILE);
    // add string

    int result = send_packet_to_fd(fd_server, &rf_packet);
    if(result == -1)
    {
        // set errno
        return -1;
    }

    int error;
    packet_t* response = wait_response_from_server(&error);
    if(error == -1)
    {
        // set errno
        return -1;
    }

    // leggo la risposta


    free(response);
    return 0;
}