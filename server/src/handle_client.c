#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "replacement_policy.h"
#include "handle_client.h"
#include "network_file.h"

#define CHECK_READ_PATH(error, req, output, is_mandatory, sender, action)\
                                error = read_data_str(req, output, MAX_PATHNAME_API_LENGTH + 1); \
                                if(error == -1) { \
                                    PRINT_WARNING_DEBUG(EINVAL, "Cannot read '" #output "' string inside packet! fd(%d)", sender); \
                                    return return_response_error(action, NULL, sender, EINVAL); \
                                } \
                                if(is_mandatory && error == 0) { \
                                    PRINT_WARNING_DEBUG(EBADMSG, "Mandatory arg! '" #output "' cannot be empty!"); \
                                    return return_response_error(action, NULL, sender, EBADMSG); \
                                }

#define CHECK_READ(error, req, data, data_size, sender, action) error = read_data(req, data, data_size); \
                                                if(error == -1) { \
                                                    PRINT_WARNING_DEBUG(EBADMSG, "Cannot read '" #data "' inside packet! fd(%d)", sender); \
                                                    return return_response_error(action, NULL, sender, EBADMSG); \
                                                }

#define RESET_FILE_WRITEMODE(file) file_set_write_enabled(file, FALSE)

packet_t* p_on_file_deleted_locks = NULL;
packet_t* p_on_file_given_lock = NULL;

static inline int return_response_error(char* action, char* pathname, int sender, int error)
{
    if(pathname)
    {
        LOG_EVENT("%s run by %d on file %s failed! [%s]", -1, action, sender, pathname, strerror(error));
    }
    else
    {
        LOG_EVENT("%s run by %d failed! [%s]", -1, action, sender, strerror(error));
    }

    errno = error;
    return error;
}

static inline void notify_given_lock(int client)
{
    if(send_packet_to_fd(client, p_on_file_given_lock) == -1)
    {
        PRINT_WARNING(errno, "Couldn't notify client on lock given! fd(%d)", client);
    }
}

static inline void notify_file_removed_to_lockers(queue_t* locks_queue)
{
    NRET_IF(!locks_queue);

    FOREACH_Q(locks_queue) {
        int client_fd = *(VALUE_IT_Q(int*));
        if(send_packet_to_fd(client_fd, p_on_file_deleted_locks) == -1)
        {
            PRINT_WARNING(errno, "Couldn't notify client locker on file removed! fd(%d)", client_fd);
        }
    }
}

static int on_files_replaced(packet_t* response, bool_t are_replaced, bool_t send_back, linked_list_t* repl_list)
{
    RET_IF(!response, -1);
    if(!are_replaced)
    {
        int zero = 0;
        if(send_back)
            write_data(response, &zero, sizeof(int));
        return 1;
    }
    RET_IF(!repl_list, -1);

    int num_files_replaced = ll_count(repl_list);
    if(send_back)
        write_data(response, &num_files_replaced, sizeof(int));

    char* files_removed_str;
    size_t char_needed = num_files_replaced * (MAX_PATHNAME_API_LENGTH + 1);
    CHECK_FATAL_EQ(files_removed_str, malloc(char_needed), NULL, NO_MEM_FATAL);

    int data_cleaned = 0;
    int files_rem_str_index = 0;
    FOREACH_LL(repl_list) {
        network_file_t* file = VALUE_IT_LL(network_file_t*);
        data_cleaned += netfile_get_data_size(file);
        notify_file_removed_to_lockers(netfile_get_locks_queue(file));

        if(send_back)
            write_netfile(response, file);

        char* file_path = netfile_get_pathname(file);
        bool_t is_last = node_get_next(CURR_IT_LL) == NULL;
        char* next_token = is_last ? "%s\0" : "%s,\0";
        snprintf(files_removed_str + files_rem_str_index, char_needed, next_token, file_path);
        files_rem_str_index += strnlen(file_path, MAX_PATHNAME_API_LENGTH) + !is_last;
    }

    ll_free(repl_list, FREE_FUNC(free_netfile));
    
    // enough length to log the entire formatted text
    size_t log_len = 150 + files_rem_str_index;
    LOG_EVENT("OP_REPLACEMENT replaced %d files and cleaned %d bytes. Files: [%s] [Success]", log_len, num_files_replaced, data_cleaned, files_removed_str);
    free(files_removed_str);
    return 1;
}

int handle_open_file_req(packet_t* req, packet_t* response)
{
    int result = 0, error;
    int sender = packet_get_sender(req);
    int flags;
    CHECK_READ(error, req, &flags, sizeof(int), sender, "OP_OPEN_FILE");
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(error, req, pathname, TRUE, sender, "OP_OPEN_FILE");

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    if(flags & O_CREATE)
    {
        if(is_file_count_full_fs(fs))
        {
            release_write_lock_fs(fs);
            return return_response_error("OP_OPEN_FILE", pathname, sender, EMLINK);
        }

        file_stored_t* file = find_file_fs(fs, pathname);
        if(file)
        {
            release_write_lock_fs(fs);
            return return_response_error("OP_OPEN_FILE", pathname, sender, EEXIST);
        }

        file = create_file(pathname);

        file_add_client(file, sender);
        if(flags & O_LOCK)
        {
            file_set_lock_owner(file, sender);
            file_set_write_enabled(file, TRUE);
        }

        if(add_file_fs(fs, pathname, file) <= 0)
        {
            release_write_lock_fs(fs);
            free_file(file);
            return return_response_error("OP_OPEN_FILE", pathname, sender, ENOMEM);
        }
    }
    else
    {
        file_stored_t* file = find_file_fs(fs, pathname);
        if(!file)
        {
            release_write_lock_fs(fs);
            return return_response_error("OP_OPEN_FILE", pathname, sender, ENOENT);
        }

        acquire_write_lock_file(file);
        if(!file_is_opened_by(file, sender))
            file_add_client(file, sender);

        if(flags & O_LOCK)
        {
            int owner = file_get_lock_owner(file);
            if(owner == -1)
                file_set_lock_owner(file, sender);
            else if(owner != sender)
            {
                file_enqueue_lock(file, sender);
                result = -1;
            }
        }

        notify_used_file(file);
        release_write_lock_file(file);
    }

    release_write_lock_fs(fs);
    LOG_EVENT("OP_OPEN_FILE run by %d on file %s with flags %d [Success]", -1, sender, pathname, flags);
    return result;
}

int handle_write_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_WRITE_FILE");
    bool_t send_back;
    CHECK_READ(read_result, req, &send_back, sizeof(bool_t), sender, "OP_WRITE_FILE");

    int data_size = packet_get_remaining_byte_count(req);
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_READ(read_result, req, data, data_size, sender, "OP_WRITE_FILE");
    }
    
    if(data_size == 0)
    {
        write_data(response, &data_size, sizeof(int));
        return 0;
    }

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_WRITE_FILE", pathname, sender, ENOENT);
    }

    acquire_read_lock_file(file);
    if(!file_is_write_enabled(file))
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_WRITE_FILE", pathname, sender, EPERM);
    }

    if(file_get_lock_owner(file) != sender)
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_WRITE_FILE", pathname, sender, EACCES);
    }

    if(is_size_too_big(fs, data_size))
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_WRITE_FILE", pathname, sender, EFBIG);
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
            release_write_lock_fs(fs);
            free(data);
            return return_response_error("OP_WRITE_FILE", pathname, sender, EFBIG);
        }
    }

    notify_memory_changed_fs(fs, data_size);

    acquire_write_lock_file(file);
    file_replace_content(file, data, data_size);
    RESET_FILE_WRITEMODE(file);
    notify_used_file(file);
    release_write_lock_file(file);

    release_write_lock_fs(fs);

    LOG_EVENT("OP_WRITE_FILE run by %d on file %s data written %d [Success]", -1, sender, pathname, data_size);
    on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
    return 0;
}

int handle_append_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_APPEND_FILE");
    bool_t send_back;
    CHECK_READ(read_result, req, &send_back, sizeof(bool_t), sender, "OP_APPEND_FILE");

    int data_size = packet_get_remaining_byte_count(req);
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_READ(read_result, req, data, data_size, sender, "OP_APPEND_FILE");
    }

    if(data_size == 0)
    {
        write_data(response, &data_size, sizeof(int));
        return 0;
    }
    
    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_APPEND_FILE", pathname, sender, ENOENT);
    }

    acquire_read_lock_file(file);
    int owner = file_get_lock_owner(file);
    if(owner != -1 && owner != sender)
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_APPEND_FILE", pathname, sender, EACCES);
    }

    if(is_size_too_big(fs, data_size))
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_APPEND_FILE", pathname, sender, EFBIG);
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
            release_write_lock_fs(fs);
            free(data);
            return return_response_error("OP_APPEND_FILE", pathname, sender, EFBIG);
        }
    }

    notify_memory_changed_fs(fs, data_size);

    acquire_write_lock_file(file);
    file_append_content(file, data, data_size);
    RESET_FILE_WRITEMODE(file);
    notify_used_file(file);
    release_write_lock_file(file);

    release_write_lock_fs(fs);

    LOG_EVENT("OP_APPEND_FILE run by %d on file %s data written %d [Success]", -1, sender, pathname, data_size);
    on_files_replaced(response, mem_missing > 0, send_back, replaced_files);
    return 0;
}

int handle_read_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_APPEND_FILE");

    file_system_t* fs = get_fs();

    acquire_read_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_read_lock_fs(fs);
        return return_response_error("OP_READ_FILE", pathname, sender, ENOENT);
    }

    acquire_read_lock_file(file);
    if(!file_is_opened_by(file, sender))
    {
        release_read_lock_file(file);
        release_read_lock_fs(fs);
        return return_response_error("OP_READ_FILE", pathname, sender, EPERM);
    }

    int owner = file_get_lock_owner(file);
    if(owner != -1 && owner != sender)
    {
        release_read_lock_file(file);
        release_read_lock_fs(fs);
        return return_response_error("OP_READ_FILE", pathname, sender, EACCES);
    }

    size_t content_size = file_get_size(file);
    if(content_size > 0)
        write_data(response, file_get_data(file), content_size);

    release_read_lock_file(file);

    acquire_write_lock_file(file);
    notify_used_file(file);
    release_write_lock_file(file);
    
    release_read_lock_fs(fs);

    LOG_EVENT("OP_READ_FILE run by %d on file %s data read %zu [Success]", -1, sender, pathname, content_size);
    return 0;
}

int handle_nread_files_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    int n_to_read;
    CHECK_READ(read_result, req, &n_to_read, sizeof(int), sender, "OP_NREAD_FILE");
    bool_t read_all = n_to_read <= 0;

    file_system_t* fs = get_fs();
    network_file_t* sent_file = create_netfile();

    acquire_read_lock_fs(fs);
    file_stored_t** files = get_files_stored(fs);
    size_t fs_file_count = get_file_count_fs(fs);
    int files_readed = read_all ? fs_file_count : MIN(n_to_read, fs_file_count);
    write_data(response, &files_readed, sizeof(int));

    // match the array boundaries (example: 4 files means -> [0, 3])
    int i = files_readed - 1;
    int data_read = 0;
    while(i >= 0)
    {
        file_stored_t* curr_file = files[i];
        acquire_read_lock_file(curr_file);
        size_t curr_size = file_get_size(curr_file);
        netfile_set_pathname(sent_file, file_get_pathname(curr_file));
        netfile_set_data(sent_file, file_get_data(curr_file), curr_size);
        write_netfile(response, sent_file);
        release_read_lock_file(curr_file);

        data_read += curr_size;
        --i;
    }
    release_read_lock_fs(fs);

    free(sent_file);
    free(files);
    LOG_EVENT("OP_READN_FILE run by %d file readed %d data read %d [Success]", -1, sender, files_readed, data_read);
    return 0;
}

int handle_remove_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_REMOVE_FILE");

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        return return_response_error("OP_REMOVE_FILE", pathname, sender, ENOENT);
    }

    acquire_read_lock_file(file);
    int owner = file_get_lock_owner(file);
    if(owner == -1 || owner != sender)
    {   
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        return return_response_error("OP_REMOVE_FILE", pathname, sender, EACCES);
    }

    int data_size = file_get_size(file);
    notify_file_removed_to_lockers(file_get_locks_queue(file));
    release_read_lock_file(file);
    remove_file_fs(fs, pathname, FALSE);
    release_write_lock_fs(fs);

    LOG_EVENT("OP_REMOVE_FILE run by %d on file %s data removed %d [Success]", -1, sender, pathname, data_size);
    return 0;
}

int handle_lock_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int result = 0;
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_REMOVE_FILE");

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        return return_response_error("OP_LOCK_FILE", pathname, sender, ENOENT);
    }

    acquire_write_lock_file(file);
    if(!file_is_opened_by(file, sender))
    {
        release_write_lock_file(file);
        release_write_lock_fs(fs);
        return return_response_error("OP_LOCK_FILE", pathname, sender, EPERM);
    }

    int owner = file_get_lock_owner(file);
    if(owner == -1)
    {
        file_set_lock_owner(file, sender);
    } else if(owner != sender)
    {
        file_enqueue_lock(file, sender);
        result = -1;
    }

    notify_used_file(file);
    release_write_lock_file(file);
    release_write_lock_fs(fs);

    LOG_EVENT("OP_LOCK_FILE run by %d on file %s lock on hold %s [Success]", -1, sender, pathname, result == -1 ? "TRUE" : "FALSE");
    return result;
}

int handle_unlock_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_REMOVE_FILE");

    file_system_t* fs = get_fs();

    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        return return_response_error("OP_UNLOCK_FILE", pathname, sender, ENOENT);
    }

    acquire_write_lock_file(file);
    if(!file_is_opened_by(file, sender))
    {
        release_write_lock_file(file);
        release_write_lock_fs(fs);
        return return_response_error("OP_UNLOCK_FILE", pathname, sender, EPERM);
    }

    int owner = file_get_lock_owner(file);
    if(owner != sender)
    {
        release_write_lock_file(file);
        release_write_lock_fs(fs);
        return return_response_error("OP_LOCK_FILE", pathname, sender, EACCES);
    }

    int new_owner = file_dequeue_lock(file);
    file_set_lock_owner(file, new_owner);
    notify_given_lock(new_owner);

    notify_used_file(file);
    release_write_lock_file(file);
    release_write_lock_fs(fs);

    LOG_EVENT("OP_UNLOCK_FILE run by %d on file %s [Success]", -1, sender, pathname);
    return 0;
}

int handle_close_file_req(packet_t* req, packet_t* response)
{
    int sender = packet_get_sender(req);
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, req, pathname, TRUE, sender, "OP_REMOVE_FILE");

    file_system_t* fs = get_fs();
    acquire_write_lock_fs(fs);
    file_stored_t* file = find_file_fs(fs, pathname);
    if(!file)
    {
        release_write_lock_fs(fs);
        return return_response_error("OP_CLOSE_FILE", pathname, sender, ENOENT);
    }

    acquire_write_lock_file(file);
    int next_owner = -1;
    if(file_get_lock_owner(file) == sender)
    {
        next_owner = file_dequeue_lock(file);
        file_set_lock_owner(file, next_owner);
    }

    file_close_client(file, sender);
    notify_used_file(file);
    release_write_lock_file(file);
    release_write_lock_fs(fs);

    if(next_owner >= 0)
        notify_given_lock(next_owner);

    LOG_EVENT("OP_CLOSE_FILE run by %d on file %s [Success]", -1, sender, pathname);
    return 0;
}