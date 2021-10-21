#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

#include "file_storage_api.h"
#include "server_api_utils.h"
#include "client_params.h"

// Check whether the result of a write is valid, return if not
#define CHECK_WRITE_PACKET(write_res) if(write_res == -1) { \
                                                PRINT_WARNING(errno, "Cannot write data inside packet!"); \
                                                return -1; \
                                            }

// Check whether the result of a read is valid, return if not
#define CHECK_READ_PACKET(read_res) if(read_res == -1) { \
                                                PRINT_WARNING(errno, "Cannot read data inside packet!"); \
                                                return -1; \
                                            }

// Write a string to the server, return if any socket error occours
#define WRITE_PACKET_STR(fd, write_res, str, len) write_res = writen_string(fd, str, len); \
                                            CHECK_WRITE_PACKET(write_res)

// Write a generic buffer of size to the server, return if any socket error occours
#define WRITE_PACKET(fd, write_res, data_ptr, size) write_res = writen(fd, data_ptr, size); \
                                            CHECK_WRITE_PACKET(write_res)

// Read a string to the server, return if any socket error occours
#define READ_PACKET_STR(fd, read_res, str, len) read_res = readn_string(fd, str, len); \
                                            CHECK_READ_PACKET(read_res)

// Read a generic buffer of size to the server, return if any socket error occours                          
#define READ_PACKET(fd, read_res, data_ptr, size) read_res = readn(fd, data_ptr, size); \
                                            CHECK_READ_PACKET(read_res)

// Return if the result of the operation is OP_ERROR, in this case set the errno and print the operation
#define RET_ON_ERROR(fd, pathname)\
                                { \
                                    server_packet_op_t op; \
                                    int read_res; \
                                    READ_PACKET(fd, read_res, &op, sizeof(server_packet_op_t)); \
                                    if(op == OP_ERROR) { \
                                        int err; \
                                        READ_PACKET(fd, read_res, &err, sizeof(int)); \
                                        errno = err; \
                                        if(g_params->print_operations) { \
                                            PRINT_INFO("%s on %s ended with failure! [%s]", __func__, pathname, strerror(err)); \
                                        } \
                                        return -1; \
                                    } \
                                }

// FD of server
int fd_server;
// First byte sent to the server
char first_byte[1] = { 0 };

// Wait until data is available from server
static int wait_response_from_server()
{
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(fd_server, &fdset);

    int res = select(fd_server + 1, &fdset, NULL, NULL, NULL);
    return res;
}

// Wait for msec
static int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
    CHECK_ERROR_EQ(fd_server, socket(AF_UNIX, SOCK_STREAM, 0), -1, -1, "Cannot connect to server!");
    long remaining_msec = (abstime.tv_sec * 1000 + abstime.tv_nsec / 1000000000) - time(0) * 1000;

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, MAX_PATHNAME_API_LENGTH);
    sa.sun_family = AF_UNIX;

    int result_socket;
    while(remaining_msec > 0 && (result_socket = connect(fd_server, (struct sockaddr*)&sa, sizeof(sa))) == -1)
    {
        remaining_msec -= msec;
        if(msleep(msec) == -1)
        {
            return -1;
        }
    }

    errno = 0;
    return result_socket;
}

int closeConnection(const char* sockname)
{
    return close(fd_server);
}

int openFile(const char* pathname, int flags)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_OPEN_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET(fd_server, error, &flags, sizeof(int));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    if(g_params->print_operations)
    {
        PRINT_INFO("openFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    return 0;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_READ_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    READ_PACKET(fd_server, error, size, sizeof(size_t));
    if(*size > 0)
    {
        CHECK_FATAL_ERRNO(*buf, malloc(*size), NO_MEM_FATAL);
        READ_PACKET(fd_server, error, *buf, *size);
    }
    else
        *buf = NULL;

    if(g_params->print_operations)
    {
        PRINT_INFO("readFile on %s ended with success, %zu bytes read! [%s]", pathname, *size, strerror(0));
    }

    return 0;
}

int readNFiles(int N, const char* dirname)
{
    int error;
    server_packet_op_t op = OP_READN_FILES;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET(fd_server, error, &N, sizeof(int));

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, "-");

    size_t num_read;
    size_t data_size_read = 0;
    READ_PACKET(fd_server, error, &num_read, sizeof(size_t));
    char full_path[MAX_PATHNAME_API_LENGTH + 1];

    char file_str[MAX_PATHNAME_API_LENGTH + 1];
    size_t file_size;
    void* file_data;

    for(int i = 0; i < num_read; ++i)
    {
        READ_PACKET_STR(fd_server, error, file_str, MAX_PATHNAME_API_LENGTH);
        READ_PACKET(fd_server, error, &file_size, sizeof(size_t));
        if(file_size > 0)
        {
            CHECK_FATAL_EQ(file_data, malloc(file_size), NULL, NO_MEM_FATAL);
            READ_PACKET(fd_server, error, file_data, file_size);
        }
        else
            file_data = NULL;

        // salva su disco
        data_size_read += file_size;
        // save file
        size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
        size_t filename_len = 0;
        char* filename = get_filename_from_path(file_str, strnlen(file_str, MAX_PATHNAME_API_LENGTH), &filename_len);
        if(buildpath(full_path, (char*)dirname, filename, dirname_len, filename_len) == -1)
        {
            PRINT_ERROR(errno, "ReadN (Saving) %s exceeded max path length (%zu)!", file_str, dirname_len + filename_len + 1);
        }
        else
        {
            if(write_file_util(full_path, file_data, file_size) == -1)
            {
                PRINT_ERROR(errno, "ReadN (Saving) %s failed!", file_str);
            }
        }
        
        free(file_data);
    }

    if(g_params->print_operations)
    {
        PRINT_INFO("readNFiles ended with success! %zu files readed for a total of %zu bytes! [%s]", num_read, data_size_read, strerror(0));
    }

    return num_read;
}

int writeFile(const char* pathname, const char* dirname)
{
    if(!pathname)
    {
        errno = EINVAL;
        return -1;
    }
    size_t path_len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);

    void* data;
    size_t data_size;
    read_file_util(pathname, &data, &data_size);

    int error;
    server_packet_op_t op = OP_WRITE_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_len);
    bool_t receive_back_files = dirname != NULL;
    WRITE_PACKET(fd_server, error, &receive_back_files, sizeof(bool_t));
    WRITE_PACKET(fd_server, error, &data_size, sizeof(size_t));
    
    if(data_size > 0)
    {
        WRITE_PACKET(fd_server, error, data, data_size);
        free(data);
    }

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    size_t num_read = 0;
    if(receive_back_files)
    {
        READ_PACKET(fd_server, error, &num_read, sizeof(size_t));
        char full_path[MAX_PATHNAME_API_LENGTH + 1];

        char file_str[MAX_PATHNAME_API_LENGTH + 1];
        size_t file_size;
        void* file_data;

        for(int i = 0; i < num_read; ++i)
        {
            READ_PACKET_STR(fd_server, error, file_str, MAX_PATHNAME_API_LENGTH);
            READ_PACKET(fd_server, error, &file_size, sizeof(size_t));
            if(file_size > 0)
            {
                CHECK_FATAL_EQ(file_data, malloc(file_size), NULL, NO_MEM_FATAL);
                READ_PACKET(fd_server, error, file_data, file_size);
            }
            else
                file_data = NULL;

            // save file
            size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
            size_t filename_len = 0;
            char* filename = get_filename_from_path(file_str, strnlen(file_str, MAX_PATHNAME_API_LENGTH), &filename_len);
            if(buildpath(full_path, (char*)dirname, filename, dirname_len, filename_len) == -1)
            {
                PRINT_ERROR(errno, "Write file (Replaced Files) %s exceeded max path length (%zu)!", file_str, dirname_len + filename_len + 1);
            }
            else
            {
                if(write_file_util(full_path, file_data, file_size) == -1)
                {
                    PRINT_ERROR(errno, "Write file (Replaced Files) %s failed!", file_str);
                }
            }
            
            free(file_data);
        }
    }

    if(g_params->print_operations)
    {
        PRINT_INFO("writeFile on %s ended with success! %zu bytes written and %zu files replaced! [%s]", 
                                pathname, data_size, num_read, strerror(0));
    }

    return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_APPEND_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);
    bool_t receive_back_files = dirname != NULL;
    WRITE_PACKET(fd_server, error, &receive_back_files, sizeof(bool_t));
    WRITE_PACKET(fd_server, error, &size, sizeof(size_t));

    if(size > 0)
    {
        WRITE_PACKET(fd_server, error, buf, size);
    }

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    size_t num_read = 0;
    if(receive_back_files)
    {
        READ_PACKET(fd_server, error, &num_read, sizeof(size_t));
        char full_path[MAX_PATHNAME_API_LENGTH + 1];

        char file_str[MAX_PATHNAME_API_LENGTH + 1];
        size_t file_size;
        void* file_data = NULL;

        for(int i = 0; i < num_read; ++i)
        {
            READ_PACKET_STR(fd_server, error, file_str, MAX_PATHNAME_API_LENGTH);
            READ_PACKET(fd_server, error, &file_size, sizeof(size_t));
            if(file_size > 0)
            {
                CHECK_FATAL_EQ(file_data, malloc(file_size), NULL, NO_MEM_FATAL);
                READ_PACKET(fd_server, error, file_data, file_size);
            }

            // save file
            size_t dirname_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);
            size_t filename_len = 0;
            char* filename = get_filename_from_path(file_str, strnlen(file_str, MAX_PATHNAME_API_LENGTH), &filename_len);
            if(buildpath(full_path, (char*)dirname, filename, dirname_len, filename_len) == -1)
            {
                PRINT_ERROR(errno, "Append file (Replaced Files) %s exceeded max path length (%zu)!", file_str, dirname_len + filename_len + 1);
            }
            else
            {
                if(write_file_util(full_path, file_data, file_size) == -1)
                {
                    PRINT_ERROR(errno, "Append file (Replaced Files) %s failed!", file_str);
                }
            }

            free(file_data);
        }
    }

    if(g_params->print_operations)
    {
        PRINT_INFO("appendFile on %s ended with success! %zu bytes written and %zu files replaced! [%s]", 
                                pathname, size, num_read, strerror(0));
    }

    return 0;
}

int closeFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_CLOSE_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    if(g_params->print_operations)
    {
        PRINT_INFO("closeFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    return 0;
}

int removeFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_REMOVE_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    if(g_params->print_operations)
    {
        PRINT_INFO("removeFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    return 0;
}

int lockFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_LOCK_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    if(g_params->print_operations)
    {
        PRINT_INFO("lockFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    return 0;
}

int unlockFile(const char* pathname)
{
    size_t path_size = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
    int error;
    server_packet_op_t op = OP_UNLOCK_FILE;
    WRITE_PACKET(fd_server, error, &first_byte, sizeof(char));
    WRITE_PACKET(fd_server, error, &op, sizeof(server_packet_op_t));
    WRITE_PACKET_STR(fd_server, error, pathname, path_size);

    CHECK_FATAL_EQ(error, wait_response_from_server(), -1, "Cannot receive response from server!");
    RET_ON_ERROR(fd_server, pathname);

    if(g_params->print_operations)
    {
        PRINT_INFO("unlockFile on %s ended with success! [%s]", pathname, strerror(0));
    }

    return 0;
}