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

void* custom_malloc(size_t size)
{
    PRINT_INFO("Allocating: %fMB", (float)((float)size/1000000));
    return malloc(size);
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