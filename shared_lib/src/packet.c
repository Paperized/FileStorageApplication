#include "packet.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "server_api_utils.h"
#include "utils.h"

struct packet_header {
    packet_op op;
    int fd_sender;
    packet_len len;
};

struct packet {
    struct packet_header header;
    char* content;
    packet_len capacity;
    packet_len cursor_index;
};

#define CHECK_READ_BYTES(bytes_read, fd, buf, size, ...) byte_read = readn(fd, buf, size); \
                                                    if(byte_read == -1) \
                                                    { \
                                                        PRINT_ERROR(errno, __VA_ARGS__); \
                                                        return NULL; \
                                                    } \
                                                    if(byte_read == 0) \
                                                    { \
                                                        packet_t* closed = create_packet(OP_CLOSE_CONN, 0); \
                                                        closed->header.fd_sender = fd; \
                                                        return closed; \
                                                    } \

/**
 * @brief Reads up to given bytes from given descriptor, saves data to given pre-allocated buffer.
 * @returns read size on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for the routine "read".
*/
static inline int readn(long fd, void* buf, size_t size)
{
	size_t left = size;
	int r;
	char* bufptr = (char*) buf;
	while (left > 0)
	{
		if ((r = read((int) fd, bufptr, left)) == -1)
		{
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0; // EOF
		left -= r;
		bufptr += r;
	}
	return size;
}

/**
 * @brief Writes buffer up to given size to given descriptor.
 * @returns 1 on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for routine "write".
*/
static inline int writen(long fd, void* buf, size_t size)
{
	size_t left = size;
	int r;
	char* bufptr = (char*) buf;
	while (left > 0)
	{
		if ((r = write((int) fd, bufptr, left)) == -1)
		{
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0;
		left -= r;
		bufptr += r;
	}
	return 1;
}

// Null connessione chiusa, != NULL packet ricevuto
packet_t* read_packet_from_fd(int fd)
{
    packet_op op;
    int byte_read;
    CHECK_READ_BYTES(byte_read, fd, &op, sizeof(packet_op), "Cannot read packet_op! fd(%d)", fd);

    packet_len len;
    CHECK_READ_BYTES(byte_read, fd, &len, sizeof(packet_len), "Cannot read packet_len! fd(%d)", fd);

    char* content = NULL;
    if(len > 0)
    {
        CHECK_FATAL_ERRNO(content, malloc(len), NO_MEM_FATAL);
        CHECK_READ_BYTES(byte_read, fd, content, len, "Cannot read packet_body! fd(%d)", fd);
    }

    packet_t* new_packet = create_packet(op, 0);
    new_packet->header.fd_sender = fd;
    new_packet->header.len = len;
    new_packet->capacity = len;
    new_packet->content = content;
    return new_packet;
}

// send the packet to a fd
int send_packet_to_fd(int fd, packet_t* p)
{
    if(!p)
    {
        errno = EINVAL;
        return -1;
    }

    int write_result;
    CHECK_ERROR_EQ(write_result, writen(fd, &p->header.op, sizeof(packet_op)), -1, -1, "Cannot write packet_op! fd(%d)", fd);
    CHECK_ERROR_EQ(write_result, writen(fd, &p->header.len, sizeof(packet_len)), -1, -1, "Cannot write packet_len! fd(%d)", fd);
    CHECK_ERROR_EQ(write_result, writen(fd, p->content, p->header.len), -1, -1, "Cannot write packet_content! fd(%d)", fd);

    return 1;
}

void clear_packet(packet_t* p)
{
    NRET_IF(!p);

    p->cursor_index = 0;
    p->header.len = 0;
}

// destroy the current packet and free its content
void destroy_packet(packet_t* p)
{
    NRET_IF(!p);

    free(p->content);
    free(p);
}

// write a data to a packet based on size and return a status, -1 the packet is null, otherwise it returns the bytes written
int write_data(packet_t* p, const void* data, size_t size)
{
    RET_IF(!data || !p, -1);
    if(size == 0)
        return 1;

    char* current_buffer = p->content;
    packet_len buff_size = p->header.len;
    packet_len free_space = p->capacity - buff_size;

    if(free_space < size)
    {
        if(buff_size == 0)
        {
            CHECK_FATAL_ERRNO(current_buffer, malloc(size), NO_MEM_FATAL);
        }
        else
        {
            CHECK_FATAL_ERRNO(current_buffer, realloc(current_buffer, p->capacity + (size - free_space)), NO_MEM_FATAL);
        }

        p->capacity = buff_size + size;
    }

    memcpy(current_buffer + buff_size, data, size);
    p->content = current_buffer;
    p->header.len = buff_size + size;
    return 1;
}

// write a string to a packet and return a status, -1 the packet is null, otherwise it returns the bytes written
int write_data_str(packet_t* p, const char* str, size_t len)
{
    RET_IF(!p || !str, -1);

    int res;

    if(str == NULL)
        len = 0;
    CHECK_ERROR_EQ(res, write_data(p, &len, sizeof(size_t)), -1, res, "Cannot write length string to packet!");

    if(str != NULL)
    {
        CHECK_ERROR_EQ(res, write_data(p, str, len), -1, res, "Cannot write string to packet!");
    }
    return res;
}

// return a pointer to the data read from the packet based on a size
// error is set to -1 if req is null, -2 if the size exceed the packet length, -3 if malloc failed due to memory issue
int read_data(packet_t* p, void* buf, size_t size)
{
    if(!p || !buf)
    {
        errno = EINVAL;
        return -1;
    }
    if(size == 0)
        return 0;

    char* current_buffer = p->content;
    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->cursor_index;

    if((cursor_index + size) > buff_size)
    {
        errno = EOVERFLOW;
        return -1;
    }

    memcpy(buf, (current_buffer + cursor_index), size);
    p->cursor_index = cursor_index + size;
    return size;
}

int read_data_str(packet_t* p, char* str, size_t input_str_length)
{
    if(!p || !str)
    {
        errno = EINVAL;
        return -1;
    }

    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->cursor_index;

    packet_len len_offset = cursor_index + sizeof(size_t);
    if(len_offset > buff_size)
    {
        errno = EOVERFLOW;
        return -1;
    }
    char* start_string_ptr = p->content + len_offset;
    size_t str_len = *((size_t*)(p->content + cursor_index));
    if(len_offset + str_len > buff_size)
    {
        errno = EOVERFLOW;
        return -1;
    }

    size_t needed_bytes = str_len * sizeof(char);
    size_t min_char = MIN(input_str_length, needed_bytes);
    memcpy(str, start_string_ptr, min_char);
    str[min_char] = '\0';
    p->cursor_index = cursor_index + sizeof(size_t) + str_len;
    return str_len;
}

// return a pointer to string read from a packet
// error is set to -1 if req is null, -2 if the size exceed the packet length, -3 if malloc failed due to memory issue, -4 if the string did not terminate
int read_data_str_alloc(packet_t* p, char** str)
{
    if(!p || !str)
    {
        errno = EINVAL;
        *str = NULL;
        return -1;
    }

    packet_len buff_size = p->header.len;
    packet_len cursor_index = p->cursor_index;

    packet_len len_offset = cursor_index + sizeof(size_t);
    if(len_offset > buff_size)
    {
        errno = EOVERFLOW;
        return -1;
    }
    char* start_string_ptr = p->content + len_offset;
    size_t str_len = *((size_t*)(p->content + cursor_index));
    if(len_offset + str_len > buff_size)
    {
        errno = EOVERFLOW;
        return -1;
    }

    size_t needed_bytes = (str_len) * sizeof(char);
    CHECK_FATAL_ERRNO(*str, malloc(needed_bytes + 1), NO_MEM_FATAL);
    memcpy(*str, start_string_ptr, needed_bytes);
    (*str)[needed_bytes] = '\0';
    p->cursor_index = cursor_index + sizeof(size_t) + str_len;
    return str_len;
}

int write_netfile(packet_t* op, network_file_t* file)
{
    RET_IF(!op || !file, -1);
    char* pathname = netfile_get_pathname(file);
    void* data = netfile_get_data(file);
    size_t data_size = netfile_get_data_size(file);

    int result;
    CHECK_ERROR_EQ(result, write_data_str(op, pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH)),
         -1, -1, "Cannot write pathname string to packet!");
    CHECK_ERROR_EQ(result, write_data(op, &data_size, sizeof(size_t)),
         -1, -1, "Cannot write data_size size_t to packet!");

    if(data_size > 0)
    {
        CHECK_ERROR_EQ(result, write_data(op, data, data_size),
         -1, -1, "Cannot write data to packet!");
    }

    return 1;
}

int read_netfile(packet_t* op, network_file_t** output)
{
    RET_IF(!op || !output, -1);
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    size_t data_size;
    void* data = NULL;

    int str_len, result;
    CHECK_ERROR_EQ(str_len, read_data_str(op, pathname, MAX_PATHNAME_API_LENGTH), -1, -1, "Cannot read pathname string from packet!");
    CHECK_ERROR_EQ(result, read_data(op, &data_size, sizeof(size_t)), -1, -1, "Cannot read data_size size_t from packet!");

    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        result = read_data(op, data, data_size);
        if(result == -1)
        {
            PRINT_ERROR(errno, "Cannot read data from packer!");
            free(data);
            *output = NULL;
            return -1;
        }
    }

    char* pathname_alloc;
    MAKE_COPY_BYTES(pathname_alloc, str_len + 1, pathname);
    *output = create_netfile();
    netfile_set_pathname(*output, pathname_alloc);
    netfile_set_data(*output, data, data_size);
    return 1;
}

int is_packet_valid(packet_t* p)
{
    RET_IF(!p, 0);

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

    return 1;
}

packet_t* create_packet(packet_op op, size_t initial_capacity)
{
    packet_t* new_p;
    CHECK_FATAL_ERRNO(new_p, malloc(sizeof(packet_t)), NO_MEM_FATAL);
    memset(new_p, 0, sizeof(packet_t));
    new_p->header.op = op;
    new_p->header.fd_sender = -1;

    new_p->capacity = initial_capacity;
    if(initial_capacity > 0) {
        CHECK_FATAL_ERRNO(new_p->content, malloc(new_p->capacity), NO_MEM_FATAL);
    }

    return new_p;
}

void print_packet(packet_t* p)
{
    NRET_IF(!p);

    printf("Packet op: %ul.\n", p->header.op);
    printf("Packet body length: %ul.\n", p->header.len);
    printf("Packet body capability: %ul.\n", p->capacity);
    printf("Packet sender: %d.\n", p->header.fd_sender);
    printf("Packet cursor index: %ul.\n", p->cursor_index);
}

int packet_get_remaining_byte_count(packet_t* p)
{
    RET_IF(!p, 0);

    return p->header.len - p->cursor_index;
}

packet_op packet_get_op(packet_t* p)
{
    RET_IF(!p, OP_UNKNOWN);

    return p->header.op;
}

int packet_get_sender(packet_t* p)
{
    RET_IF(!p, 0);

    return p->header.fd_sender;
}

packet_len packet_get_length(packet_t* p)
{
    RET_IF(!p, 0);

    return p->header.len;
}

void packet_set_op(packet_t* p, packet_op op)
{
    NRET_IF(!p);

    p->header.op = op;
}

void packet_set_sender(packet_t* p, int sender)
{
    NRET_IF(!p);
    
    p->header.fd_sender = sender;
}