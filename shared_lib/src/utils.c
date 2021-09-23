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

int read_file_util(const char* pathname, void** buffer, size_t* size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "r"), NULL, -1, "Cannot read file from disk!");
    int curr_size;
    CHECK_ERROR_EQ(curr_size, get_file_size(f), -1, -1, "Cannot find file length from disk!");
    CHECK_FATAL_EQ(*buffer, malloc(curr_size), NULL, NO_MEM_FATAL);

    *size = curr_size;
    int res = curr_size > 0 ? fread(*buffer, *size, 1, f) : 0;
    fclose(f);

    if(res == 0)
        free(*buffer);

    return res;
}

int write_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "w"), NULL, -1, "Cannot write file to disk!");
    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fclose(f);
    return res;
}

int append_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f;
    CHECK_ERROR_EQ(f, fopen(pathname, "a"), NULL, -1, "Cannot append file to disk!");
    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fclose(f);
    return res;
}

int buildpath(char* dest, char* src1, char* src2, size_t src1length, size_t src2length)
{
    if(src1length + src2length + 1 > MAX_PATHNAME_API_LENGTH)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    strncat(dest, src1, src1length);
    dest[src1length] = '/';
    strncat(dest + src1length + 1, src2, src2length);
    return 1;
}