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

#define RESET_FILE_WRITEMODE(file) file_set_write_enabled(file, FALSE)

packet_t* p_on_file_deleted_locks = NULL;

static inline void notify_file_removed_to_lockers(queue_t* locks_queue)
{
    NRET_IF(!locks_queue);

    node_t* curr = get_head_node_q(locks_queue);
    while(curr)
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

    while(curr_file)
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
        file_add_client(file, sender);
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
        if(!file_is_opened_by(file, sender))
            file_add_client(file, sender);

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
    
    RET_IF(data_size == 0, 0);

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return ENOENT;
    }

    acquire_read_lock_file(file);
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
    release_read_lock_file(file);

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

    notify_memory_changed_fs(fs, data_size);
    file_replace_content(file, data, data_size);
    RESET_FILE_WRITEMODE(file);
    release_write_lock_fs(fs);

    on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
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

    RET_IF(data_size == 0, 0);
    
    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return ENOENT;
    }

    acquire_read_lock_file(file);
    int owner = file_get_lock_owner(file);
    if(owner != -1 && owner != sender)
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
    release_read_lock_file(file);

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

    notify_memory_changed_fs(fs, data_size);
    file_append_content(file, data, data_size);
    RESET_FILE_WRITEMODE(file);
    release_write_lock_fs(fs);

    on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
    return 0;
}

int handle_read_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    READ_PATH(read_result, req, pathname, TRUE, "Cannot read pathname inside packet! fd(%d)", sender)

    file_system_t* fs = get_fs();

    acquire_read_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_read_lock_fs(fs);
        return ENOENT;
    }

    acquire_read_lock_file(file);
    release_read_lock_fs(fs);
    if(!file_is_opened_by(file, sender))
    {
        release_read_lock_file(file);
        return EPERM;
    }

    int owner = file_get_lock_owner(file);
    if(owner != -1 && owner != sender)
    {
        release_write_lock_fs(fs);
        return EACCES;
    }

    size_t content_size = file_get_size(file);
    if(content_size > 0)
        write_data(response, file_get_data(file), content_size);
    release_read_lock_file(file);
    return 0;
}

int handle_nread_files_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    size_t n_to_read;
    CHECK_WARNING_EQ_ERRNO(read_result, read_data(req, &n_to_read, sizeof(size_t)), -1, EBADMSG,
                                    EBADMSG, "Cannot read n_to_read inside packet! fd(%d)", sender);
    bool_t read_all = n_to_read <= 0;

    file_system_t* fs = get_fs();

    acquire_read_lock_fs(fs);
    file_stored_t** files = get_files_stored(fs);
    size_t fs_file_count = get_file_count_fs(fs);
    size_t files_readed = read_all ? fs_file_count : MIN(n_to_read, fs_file_count);
    write_data(response, &files_readed, sizeof(size_t));

    // match the array boundaries (example: 4 files means -> [0, 3])
    files_readed -= 1;
    while(read_all || files_readed >= 0)
    {
        file_stored_t* curr_file = files[files_readed];
        acquire_read_lock_file(curr_file);
        char* curr_pathname = file_get_pathname(curr_file);
        size_t curr_pathlen = strnlen(curr_pathname, MAX_PATHNAME_API_LENGTH);
        write_data_str(response, curr_pathname, curr_pathlen);
        size_t curr_data_size = file_get_size(curr_file);
        write_data(response, &curr_data_size, sizeof(size_t));
        if(curr_data_size > 0)
            write_data(response, file_get_data(curr_file), curr_data_size);
        release_read_lock_file(curr_file);
        --files_readed;
    }

    release_read_lock_fs(fs);
    free(files);
    return 0;
}

int handle_remove_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH];
    READ_PATH(read_result, req, pathname, TRUE, "Cannot read pathname inside packet! fd(%d)", sender);

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        return ENOENT;
    }

    int owner = file_get_lock_owner(file);
    if(owner == -1 || owner != sender)
    {
        release_write_lock_fs(fs);
        return EACCES;
    }

    notify_file_removed_to_lockers(file_get_locks_queue(file));
    remove_file_fs(fs, pathname, FALSE);
    release_write_lock_fs(fs);
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