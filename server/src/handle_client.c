#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "replacement_policy.h"
#include "handle_client.h"

#define READ_PATH(byte_read, pk, output, is_mandatory, ...) CHECK_WARNING_EQ_ERRNO(byte_read, read_data_str(pk, output, MAX_PATHNAME_API_LENGTH), -1, EINVAL, EINVAL, __VA_ARGS__); \
                                                        if(is_mandatory && byte_read == 0) { \
                                                            PRINT_WARNING(EBADMSG, "Mandatory arg! " # __VA_ARGS__); \
                                                            errno = EBADMSG; \
                                                            return EBADMSG; \
                                                        }

packet_t* p_on_file_deleted_locks = NULL;

static inline void notify_file_removed_to_lockers(queue_t* locks_queue)
{
    NRET_IF(!locks_queue);

    node_t* curr = get_head_node_q(locks_queue);
    while(!curr)
    {
        int client_fd = *((int*)node_get_value(curr));
        if(send_packet_to_fd(client_fd, p_on_file_deleted_locks) == -1)
        {
            PRINT_WARNING(errno, "Couldn't notify client locker on file removed! fd(%d)", client_fd);
        }

        curr = node_get_next(curr);
    }
}

static int on_files_replaced(packet_t* response, bool_t are_replaced, bool_t send_back, linked_list_t* repl_list)
{
    RET_IF(!are_replaced, 1);
    RET_IF(!response || !repl_list, -1);

    if(send_back)
    {
        int num_files_replaced = ll_count(repl_list);
        write_data(response, &num_files_replaced, sizeof(int));
    }
    node_t* curr_file = ll_get_head_node(repl_list);

    while(!curr_file)
    {
        replacement_entry_t* file = node_get_value(curr_file);
        notify_file_removed_to_lockers(repl_get_locks_queue(file));

        if(send_back)
        {
            char* filename_replaced = repl_get_pathname(file);
            size_t len_filename = strnlen(filename_replaced, MAX_PATHNAME_API_LENGTH);
            size_t data_size = repl_get_data_size(file);
            void* data = repl_get_data(file);

            write_data_str(response, filename_replaced, len_filename);
            write_data(response, &data_size, sizeof(size_t));
            write_data(response, data, data_size);
        }

        curr_file = node_get_next(curr_file);
    }

    ll_free(repl_list, free_repl);
    return 1;
}

int handle_open_file_req(packet_t* req, packet_t* response)
{
    int result = 0, error;
    int sender = packet_get_sender(req);
    int flags;
    CHECK_WARNING_EQ_ERRNO(error, read_data(req, &flags, sizeof(int)), -1, EBADMSG,
                                 EBADMSG, "Cannot read flags inside packet! fd(%d)", sender);
    char pathname[MAX_PATHNAME_API_LENGTH];
    READ_PATH(error, req, pathname, TRUE, "Cannot read pathname inside packet! fd(%d)", sender);

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

        file = create_file(pathname);
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
    READ_PATH(read_result, req, pathname, TRUE, "Cannot read pathname inside packet! fd(%d)", sender);
    bool_t send_back;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &send_back, sizeof(bool_t)), -1, EBADMSG,
                                 EBADMSG, "Cannot read 'send back' inside packet! fd(%d)", sender);

    int data_size = packet_get_remaining_byte_count(req);
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, data, data_size), -1, EBADMSG,
                                 EBADMSG, "Cannot read buffer file inside packet! fd(%d)", sender);
    }
    
    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return ENOENT;
    }

    if(!file_is_write_enabled(file))
    {
        release_write_lock_fs(fs);
        free(data);
        return EPERM;
    }

    if(file_get_lock_owner(file) != sender)
    {
        release_write_lock_fs(fs);
        free(data);
        return EACCES;
    }

    if(is_size_too_big(fs, data_size))
    {
        release_write_lock_fs(file);
        free(data);
        return EFBIG;
    }

    int mem_missing = is_size_available(fs, data_size);
    linked_list_t* replaced_files = NULL;

    // CACHE REPLACEMENT
    if(mem_missing > 0)
    {
        bool_t success = run_replacement_algorithm(pathname, mem_missing, &replaced_files);
        if(!success)
        {
            release_write_lock_fs(file);
            free(data);
            return EFBIG;
        }
    }

    acquire_write_lock_file(file);
    release_write_lock_fs(fs);
    file_replace_content(file, data, data_size);
    file_set_write_enabled(file, FALSE);
    release_write_lock_file(file);

    int res = on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
    return 0;
}

int handle_append_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    READ_PATH(read_result, req, pathname, TRUE, "Cannot read pathname inside packet! fd(%d)", sender);
    bool_t send_back;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &send_back, sizeof(bool_t)), -1, EBADMSG,
                                 EBADMSG, "Cannot read 'send back' inside packet! fd(%d)", sender);

    int data_size = packet_get_remaining_byte_count(req);
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, data, data_size), -1, EBADMSG,
                                 EBADMSG, "Cannot read buffer file inside packet! fd(%d)", sender);
    }
    
    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return ENOENT;
    }

    if(!file_is_write_enabled(file))
    {
        release_write_lock_fs(fs);
        free(data);
        return EPERM;
    }

    if(file_get_lock_owner(file) != sender)
    {
        release_write_lock_fs(fs);
        free(data);
        return EACCES;
    }

    if(is_size_too_big(fs, data_size))
    {
        release_write_lock_fs(file);
        free(data);
        return EFBIG;
    }

    int mem_missing = is_size_available(fs, data_size);
    linked_list_t* replaced_files = NULL;

    // CACHE REPLACEMENT
    if(mem_missing > 0)
    {
        bool_t success = run_replacement_algorithm(pathname, mem_missing, &replaced_files);
        if(!success)
        {
            release_write_lock_fs(file);
            free(data);
            return EFBIG;
        }
    }

    acquire_write_lock_file(file);
    release_write_lock_fs(fs);
    file_replace_content(file, data, data_size);
    file_set_write_enabled(file, FALSE);
    release_write_lock_file(file);

    int res = on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
    ll_free(replaced_files, free_repl);
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