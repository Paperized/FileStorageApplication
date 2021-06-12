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
    FILE* f = fopen(pathname, "r");
    if(f == NULL)
        return -1;

    int curr_size = get_file_size(f);
    if(curr_size == -1)
        return -1;

    *buffer = malloc(curr_size);
    if(*buffer == NULL)
        return -1;

    *size = curr_size;

    int res = curr_size > 0 ? fread(*buffer, *size, 1, f) : 0;
    fclose(f);

    if(res == 0)
        free(*buffer);

    return res;
}

int write_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f = fopen(pathname, "w");
    if(f == NULL)
        return -1;

    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fflush(f);
    fclose(f);

    return res;
}

int append_file_util(const char* pathname, void* buffer, size_t size)
{
    FILE* f = fopen(pathname, "a");
    if(f == NULL)
        return -1;

    int res = size > 0 ? fwrite(buffer, size, 1, f) : 0;
    fclose(f);

    return res;
}

void extract_dirname_and_filename(const char* fullpath, char** dir, char** fn)
{
    int len = strnlen(fullpath, MAX_PATHNAME_API_LENGTH);
    char* fullpath_copy = malloc(sizeof(char) * len);
    strncpy(fullpath_copy, fullpath, len);
    *fn = basename(fullpath_copy);

    fullpath_copy = malloc(sizeof(char) * len);
    strncpy(fullpath_copy, fullpath, len);
    *dir = dirname(fullpath_copy);

    free(fullpath_copy);
}