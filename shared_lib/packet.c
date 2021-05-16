#include "packet.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define CHECK_PACKET2(p) if(p == NULL) return 0
#define CHECK_PACKET(p) if(p == NULL) return

void* read_n_bytes_from_fd(int fd, ssize_t size)
{
    ssize_t curr_bytes_read = 0;
    char* buf = malloc(size);

    /* NULL means connection closed by the other host */
    int n_readed = read(fd, buf + curr_bytes_read, size - curr_bytes_read);
    if(n_readed == 0)
        return NULL;
    else if(n_readed > 0)
        curr_bytes_read = n_readed;

    while(curr_bytes_read != size)
    {
        int n_readed = read(fd, buf + curr_bytes_read, size - curr_bytes_read);
        if(n_readed != -1)
            curr_bytes_read += n_readed;
    }

    return buf;
}

int write_n_bytes_to_fd(int fd, char* buf, ssize_t size)
{
    ssize_t curr_bytes_wrote = 0;

    /* NULL means connection closed by the other host */
    int n_wrote = read(fd, buf + curr_bytes_wrote, size - curr_bytes_wrote);
    if(n_wrote == 0)
        return 0;
    else if(n_wrote > 0)
        curr_bytes_wrote = n_wrote;

    while(curr_bytes_wrote != size)
    {
        int n_wrote = write(fd, buf + curr_bytes_wrote, size - curr_bytes_wrote);
        if(n_wrote != -1)
            curr_bytes_wrote += n_wrote;
    }

    return curr_bytes_wrote;
}

// Null connessione chiusa, != NULL packet ricevuto
packet_t* read_packet_from_fd(int fd)
{
    packet_op* op = read_n_bytes_from_fd(fd, sizeof(packet_op));
    if(op == NULL)
        return NULL;
    packet_len* len = read_n_bytes_from_fd(fd, sizeof(packet_len));
    char* content = NULL;

    if(*len > 0)
        content = read_n_bytes_from_fd(fd, *len);

    packet_t* new_packet = malloc(sizeof(packet_t));
    new_packet->header.op = *op;
    new_packet->header.fd_sender = fd;
    new_packet->header.len = *len;
    new_packet->body.content = content;
    free(op);
    free(len);
    return new_packet;
}

int send_packet_to_fd(int fd, packet_t* p)
{
    CHECK_PACKET2(p);

    size_t packet_length = sizeof(packet_op) + sizeof(packet_len) + p->header.len;
    char* buffer = malloc(packet_length);
    memcpy(buffer, &p->header.op, sizeof(packet_op));
    memcpy(buffer + sizeof(packet_op), &p->header.len, sizeof(packet_len));
    memcpy(buffer + sizeof(packet_op) + sizeof(packet_len), &p->body.content, p->header.len);

    int res = write_n_bytes_to_fd(fd, buffer, packet_length);
    if(res == 0)
        return 0;

    free(buffer);
    return 1;
}

void destroy_packet(packet_t* p)
{
    CHECK_PACKET(p);
    free(p->body.content);
    free(p);
}

int is_packet_valid(packet_t* p)
{
    packet_op op = p->header.op;
    switch (op)
    {
    case OP_APPEND_FILE:
    case OP_CLOSE_CONN:
    case OP_OPEN_FILE:
    case OP_READ_FILE:
    case OP_REMOVE_FILE:
    case OP_WRITE_FILE:
        return 1;
        break;
    }

    return 0;
}