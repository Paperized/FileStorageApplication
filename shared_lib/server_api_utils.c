#include "server_api_utils.h"

const char* get_error_str(server_errors_t err)
{
    switch(err)
    {
        case ERR_FILE_ALREADY_EXISTS:
            return "ERR_FILE_ALREADY_EXISTS";
        case ERR_PATH_NOT_EXISTS:
            return "ERR_PATH_NOT_EXISTS (file or dir does not exists)";
        case ERR_FILE_NOT_OPEN:
            return "ERR_FILE_NOT_OPEN";
        default:
            return "Unknown (?)";
    }
}