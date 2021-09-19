#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "handle_client.h"

int handle_open_file_req(packet_t* req, packet_t* response)
{
    int result = 0, error;
    int sender = packet_get_sender(req);
    int flags;
    CHECK_WARNING_EQ_ERRNO(error, read_data(req, &flags, sizeof(int)), -1, EBADF,
                                 EBADF, "Cannot read flags inside packet! fd(%d)", sender);
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(error, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, EBADF,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    PRINT_INFO("Pathname %s.", pathname);

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    if(flags | O_CREATE)
    {
        if(is_file_count_full_fs(fs))
        {
            release_write_lock_fs(fs);
            return EMLINK;
        }

        file_stored_t* file = find_file_fs(fs, pathname);
        if(file)
        {
            release_write_lock_fs(fs);
            return EEXIST;
        }

        file = create_file();
        if(flags | O_LOCK)
        {
            file_set_lock_owner(file, sender);
            file_set_write_enabled(file, TRUE);
        }

        if(!add_file_fs(fs, pathname, file))
        {
            release_write_lock_fs(fs);
            free_file(file);
            return ENOMEM;
        }
    }
    else if(flags | O_LOCK)
    {
        file_stored_t* file = find_file_fs(fs, pathname);
        if(!file)
        {
            release_write_lock_fs(fs);
            return ENOENT;
        }

        acquire_write_lock_file(file);
        int owner = file_get_lock_owner(file);
        if(owner == -1)
            file_set_lock_owner(file, sender);
        else if(owner != sender)
        {
            file_enqueue_lock(file, sender);
            result = -1;
        }
        release_write_lock_file(file);
    }

    release_write_lock_fs(fs);
    return result;
}

int handle_write_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    return 0;
}

int handle_append_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    size_t buff_size = 0;
    void* buffer = NULL;

    return 0;
}

int handle_read_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    return 0;
}

int handle_nread_files_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    int n_to_read;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &n_to_read, sizeof(int)), -1, -1,
                                    EBADF, "Cannot read n_to_read inside packet! fd(%d)", sender);
    bool_t read_all = n_to_read <= 0;
    char dirname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, dirname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read dirname inside packet! fd(%d)", sender);

    return 0;
}

int handle_remove_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);

    return 0;
}

int handle_lock_file_req(packet_t* req, packet_t* response)
{
    return 0;
}

int handle_unlock_file_req(packet_t* req, packet_t* response)
{
    return 0;
}

int handle_close_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    CHECK_WARNING_EQ_ERRNO(read_result, read_data_str(req, pathname, MAX_PATHNAME_API_LENGTH), -1, -1,
                                 EBADF, "Cannot read pathname inside packet! fd(%d)", sender);


    return 0;
}