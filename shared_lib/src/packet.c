#include "packet.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "server_api_utils.h"

#define CHECK_PACKET2(p) if(p == NULL) return 0
#define CHECK_PACKET(p) if(p == NULL) return

#define DWRITE_BYTES_TO_FD(fd, buf, size, res) res = write_n_bytes_to_fd(fd, buf, size); \
                                                if(res != size) \
                                                    printf("[Warning]: DWRITE_BYTES_TO_FD did write %d bytes to %lu.\n", res, size);

void* read_n_bytes_from_fd(int fd, ssize_t size, int* error)
{
    ssize_t curr_bytes_read = 0;
    char* buf = malloc(size);

    /* NULL means connection closed by the other host */
    int n_readed = read(fd, buf + curr_bytes_read, size - curr_bytes_read);
    if(n_readed < 0)
    {
        free(buf);
        *error = -1;
        return NULL;
    }
    else if(n_readed == 0)
    {
        free(buf);
        *error = 0;
        return NULL;
    }
    else
        curr_bytes_read = n_readed;

    while(curr_bytes_read != size)
    {
        int n_readed = read(fd, buf + curr_bytes_read, size - curr_bytes_read);
        if(n_readed == -1)
        {
            free(buf);
            *error = -1;
            return NULL;
        }

        curr_bytes_read += n_readed;
    }

    *error = 0;
    return buf;
}

int write_n_bytes_to_fd(int fd, char* buf, ssize_t size)
{
    ssize_t curr_bytes_wrote = 0;

    /* NULL means connection closed by the other host */
    int n_wrote = write(fd, buf + curr_bytes_wrote, size - curr_bytes_wrote);
    if(n_wrote < 0)
        return -1;
    else if(n_wrote == 0)
        return 0;
    else
        curr_bytes_wrote = n_wrote;

    while(curr_bytes_wrote != size)
    {
        int n_wrote = write(fd, buf + curr_bytes_wrote, size - curr_bytes_wrote);
        if(n_wrote != -1)
            return -1;
        
        curr_bytes_wrote += n_wrote;
    }

    return curr_bytes_wrote;
}

// Null connessione chiusa, != NULL packet ricevuto
packet_t* read_packet_from_fd(int fd, int* error)
{
    packet_op* op = read_n_bytes_from_fd(fd, sizeof(packet_op), error);
    if(*error == -1)
    {
        printf("Error reading op packet.\n");
        return NULL;
    }

    // connessione chiusa
    if(op == NULL)
    {
        packet_t* closed = create_packet(OP_CLOSE_CONN);
        closed->header.fd_sender = fd;
        return closed;
    }

    packet_len* len = read_n_bytes_from_fd(fd, sizeof(packet_len), error);
    if(*error == -1)
    {
        free(op);
        return NULL;
    }

    char* content = NULL;
    if(len != NULL)
    {
        if(*len > 0)
            content = read_n_bytes_from_fd(fd, *len, error);

        if(*error == -1)
        {
            free(op);
            free(len);
            return NULL;
        }
    }
    else
    {
        len = malloc(sizeof(packet_len));
        *len = 0;
    }

    packet_t* new_packet = create_packet(*op);
    new_packet->header.fd_sender = fd;
    new_packet->header.len = *len;
    new_packet->body.content = content;
    free(op);
    free(len);
    return new_packet;
}

// send the packet to a fd
int send_packet_to_fd(int fd, packet_t* p)
{
    CHECK_PACKET2(p);

    size_t packet_length = sizeof(packet_op) + sizeof(packet_len) + p->header.len;
    char* buffer = malloc(packet_length);
    memcpy(buffer, &p->header.op, sizeof(packet_op));
    memcpy((buffer + 4), &p->header.len, 4);
    memcpy((buffer + 8), p->body.content, p->header.len);

    int res;
    DWRITE_BYTES_TO_FD(fd, buffer, packet_length, res);
    free(buffer);
    return res;
}

// destroy the current packet and free its content
void destroy_packet(packet_t* p)
{
    CHECK_PACKET(p);

    if(p->body.content)
        free(p->body.content);
    free(p);
}

// write a data to a packet based on size and return a status, -1 the packet is null, otherwise it returns the bytes written
int write_data(packet_t* p, const void* data, size_t size)
{
    if(data == NULL)
        return -1;

    char* current_buffer = p->body.content;
    packet_len buff_size = p->header.len;

    if(buff_size == 0)
    {
        current_buffer = malloc(size);
        if(current_buffer == NULL)
        {
            return -2;
        }
    }
    else
    {
        current_buffer = realloc(current_buffer, buff_size + size);
        if(current_buffer == NULL)
        {
            return -2;
        }
    }

    memcpy(current_buffer + buff_size, data, size);
    p->body.content = current_buffer;
    p->header.len = buff_size + size;
    return 0;
}

// write a string to a packet and return a status, -1 the packet is null, otherwise it returns the bytes written
int write_data_str(packet_t* p, const char* str)
{
    if(p == NULL)
        return -1;

    size_t len = strnlen(str, MAX_PATHNAME_API_LENGTH) + 1;
    int res = write_data(p, str, len);
    if(res < 0)
        return res;

    p->body.content[p->header.len - 1] = '\0';
    return res;
}

// return a pointer to the data read from the packet based on a size
// error is set to -1 if req is null, -2 if the size exceed the packet length, -3 if malloc failed due to memory issue
void* read_data(packet_t* p, size_t size, int* error)
{
    if(p == NULL)
    {
        *error = -1;
        return NULL;
    }

    char* current_buffer = p->body.content;
    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->body.cursor_index;

    if((cursor_index + size) > buff_size)
    {
        *error = -2;
        return NULL;
    }

    char* readed_data = malloc(size);
    if(readed_data == NULL)
    {
        *error = -3;
        return NULL;
    }

    memcpy(readed_data, (current_buffer + cursor_index), size);
    p->body.cursor_index = cursor_index + size;
    *error = 0;
    return readed_data;
}

// read the entire packet content left and set the bytes read, eventually setting error to -1 if the packet is NULL
void* read_until_end(packet_t* p, size_t* size_read, int* error)
{
    if(p == NULL)
    {
        *error = -1;
        return NULL;
    }

    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->body.cursor_index;

    void* data = read_data(p, buff_size - cursor_index, error);
    *size_read = data == NULL ? 0 : buff_size - cursor_index;
    return data;
}

// return a pointer to string read from a packet
// error is set to -1 if req is null, -2 if the size exceed the packet length, -3 if malloc failed due to memory issue, -4 if the string did not terminate
char* read_data_str(packet_t* p, int* error)
{
    if(p == NULL)
    {
        *error = -1;
        return NULL;
    }

    char* current_buffer = p->body.content;
    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->body.cursor_index;

    size_t current_str_length = 0;
    size_t next_curs_index;
    while((next_curs_index = cursor_index + current_str_length) <= buff_size && *(current_buffer + next_curs_index) != '\0')
        current_str_length += 1;

    // null char
    current_str_length += 1;
    next_curs_index += 1;
    if(next_curs_index > buff_size)
    {
        *error = -2;
        return NULL;
    }

    if(*(current_buffer + next_curs_index) != '\0')
    {
        *error = -4;
        return NULL;
    }

    size_t needed_bytes = current_str_length * sizeof(char);
    char* readed_str = malloc(needed_bytes);
    if(readed_str == NULL)
    {
        *error = -3;
        return NULL;
    }

    memcpy(readed_str, (current_buffer + cursor_index), needed_bytes);
    p->body.cursor_index = cursor_index + needed_bytes;
    *error = 0;
    return readed_str;
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
    case OP_READN_FILES:
    case OP_OK:
    case OP_ERROR:
        return 1;
        break;
    }

    return 0;
}

packet_t* create_packet(packet_op op)
{
    packet_t* new_p = malloc(sizeof(packet_t));
    memset(new_p, 0, sizeof(packet_t));
    new_p->header.op = op;
    new_p->header.fd_sender = -1;

    return new_p;
}

void print_packet(packet_t* p)
{
    if(p == NULL) return;

    printf("Packet op: %ul.\n", p->header.op);
    printf("Packet body length: %ul.\n", p->header.len);
    printf("Packet sender: %d.\n", p->header.fd_sender);
    printf("Packet cursor index: %ul.\n", p->body.cursor_index);
}