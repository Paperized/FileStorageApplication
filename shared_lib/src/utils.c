#include "utils.h"

#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include "server_api_utils.h"

int get_file_size(FILE* f)
{
    if(fseek(f, 0, SEEK_END) != 0)
        return -1;

    int size = ftell(f);

    rewind(f);
    return size;
}

bool_t is_valid_op(server_packet_op_t op)
{
    return op >= OP_OPEN_FILE && op <= OP_OK;
}

int read_file_util(const char* pathname, void** buffer, size_t* size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "r"), NULL, -1, "Cannot read file %s from disk!", pathname);
    int curr_size;
    CHECK_ERROR_EQ(curr_size, get_file_size(f), -1, -1, "Cannot find file length %s from disk!", pathname);
    if(curr_size > 0)
    {
        CHECK_FATAL_EQ(*buffer, malloc(curr_size), NULL, NO_MEM_FATAL);
    }
    else
    {
        *buffer = NULL;
    }

    *size = curr_size;
    int res = curr_size > 0 ? fread(*buffer, *size, 1, f) : 0;
    fclose(f);

    return res;
}

int write_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "w"), NULL, -1, "Cannot write file %s to disk!", pathname);
    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fclose(f);
    return res;
}

int append_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "a"), NULL, -1, "Cannot append file %s to disk!", pathname);
    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fclose(f);
    return res;
}

char* get_filename_from_path(const char* path, size_t path_len, size_t* filename_len)
{
    RET_IF(!path || path_len == 0, NULL);
    size_t curr_index = path_len;
    char curr_char;
    while(--curr_index >= 0 && (curr_char = path[curr_index]) != '/');
    if(curr_char == '/')
    {
        if(filename_len)
            *filename_len = path_len - curr_index - 1;
        return (char*)(path + curr_index + 1);
    }

    if(filename_len)
        *filename_len = path_len;
    return (char*)path;
}

int buildpath(char* dest, const char* src1, const char* src2, size_t src1length, size_t src2length)
{
    if(src1length + src2length + 1 > MAX_PATHNAME_API_LENGTH)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(dest, src1, src1length);
    dest[src1length] = '/';
    memcpy(dest + src1length + 1, src2, src2length);
    dest[src1length + src2length + 1] = '\0';
    return 1;
}

static inline bool_t is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline int n_atoi(const char* str, int len)
{
    int ret = 0;
    for(int i = 0; i < len; ++i)
    {
        ret = ret * 10 + (str[i] - '0');
    }
    return ret;
}

int filesize_string_to_byte(char* str, unsigned int max_length)
{
    RET_IF(!str, 0);

    size_t len = strnlen(str, max_length);
    int i = 0;
    while(i < len && is_digit(str[i])) ++i;
    if(i == len)
    {
        PRINT_INFO("No unit of measure specified, BYTE choosen by default!");
        return atoi(str);
    }

    const char* str_unit = (const char*)(str + i);
    int unit_len = len - i;

    if(unit_len == 1 && str_unit[0] == 'B')
        return n_atoi(str, len - 1);
    
    if(unit_len > 2)
    {
        PRINT_INFO("Not recognized unit of measure, BYTE choosen by default!");
        return n_atoi(str, len - unit_len);
    }

    if(strncmp(str_unit, "KB", 2) == 0)
        return n_atoi(str, len - 2) * 1000;

    if(strncmp(str_unit, "MB", 2) == 0)
        return n_atoi(str, len - 2) * 1000 * 1000;

    if(strncmp(str_unit, "GB", 2) == 0)
        return n_atoi(str, len - 2) * 1000 * 1000 * 1000;

    if(strncmp(str_unit, "TB", 2) == 0)
        return n_atoi(str, len - 2) * 1000 * 1000 * 1000 * 1000;

    PRINT_INFO("Not recognized unit of measure, BYTE choosen by default!");
    return n_atoi(str, len - 2);
}

/**
 * @brief Reads up to given bytes from given descriptor, saves data to given pre-allocated buffer.
 * @returns read size on success, -1 on failure.
 * @exception The function may fail and set "errno" for any of the errors specified for the routine "read".
*/
int readn(long fd, void* buf, size_t size)
{
    RET_IF(size == 0, 0);
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
int writen(long fd, void* buf, size_t size)
{
    RET_IF(size == 0, 1);
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

int readn_string(long fd, char* buf, size_t max_len)
{
    RET_IF(!buf || max_len == 0, 0);
    size_t str_len;
    int res = readn(fd, &str_len, sizeof(size_t));
    if(res <= 0)
        return res;
    if(str_len == 0)
        return 0;
    
    size_t actual_len = MIN(str_len, max_len);
    res = readn(fd, buf, actual_len);
    if(res < 0)
        return res;
    
    buf[actual_len] = '\0';
    return actual_len;
}

int writen_string(long fd, const char* buf, size_t len)
{
    if(writen(fd, &len, sizeof(size_t)) == -1)
        return -1;

    if(len == 0)
    {
        return 1;
    }

    return writen(fd, (void*)buf, len);
}