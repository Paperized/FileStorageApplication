#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "server.h"
#include "replacement_policy.h"
#include "handle_client.h"
#include "replaced_file.h"

// Read a string(in this program we just read pathnames) for a max of MAX_PATHNAME_API_LENGTH characters
// If the return status is -1 a problem occured with the sender (probably connection closed) and we return with an error
// If the return status is 0 the server was not able to read anything(we consider any param required) so we return with an error
#define CHECK_READ_PATH(error, output, sender, action)\
                                error = readn_string(sender, output, MAX_PATHNAME_API_LENGTH); \
                                if(error == -1) { \
                                    PRINT_WARNING_DEBUG(EINVAL, "Cannot read '" #output "' string inside packet! fd(%d)", sender); \
                                    return return_response_error(action, NULL, sender, EINVAL); \
                                } \
                                if(error == 0) { \
                                    PRINT_WARNING_DEBUG(EBADMSG, "Mandatory arg! '" #output "' cannot be empty! fd(%d)", sender); \
                                    return return_response_error(action, NULL, sender, EBADMSG); \
                                }

// Read some data into the data buffer with length data_size
// If the return status is -1 a problem occured with the sender (probably connection closed) and we return with an error
// If the return status is 0 the server was not able to read anything(we consider any param required) so we return with an error
#define CHECK_READ(error, data, data_size, sender, action) error = readn(sender, data, data_size); \
                                                            if(error == -1) \
                                                            { \
                                                                PRINT_WARNING_DEBUG(EINVAL, "Cannot read '" #data "' string inside packet! fd(%d)", sender); \
                                                                return return_response_error(action, NULL, sender, EINVAL); \
                                                            } \
                                                            if(error == 0) \
                                                            { \
                                                                PRINT_WARNING_DEBUG(EBADMSG, "Mandatory arg! '" #data "' cannot be empty! fd(%d)", sender); \
                                                                return return_response_error(action, NULL, sender, EBADMSG); \
                                                            }

#define RESET_FILE_WRITEMODE(file) file_set_write_enabled(file, FALSE)

// Used by the server api handlers on error, logs the failed action, send back the error and set the errno value
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

    server_packet_op_t res_op = OP_ERROR;
    if(writen(sender, &res_op, sizeof(res_op)))
        writen(sender, &error, sizeof(error));
    errno = error;
    return error;
}

void notify_given_lock(int client)
{
    server_packet_op_t op = OP_OK;
    writen(client, &op, sizeof(op));
}

// Used in handle_remove_file_req(sender), cleanup the lock queue and send back an OP_ERROR to each of them
static inline void notify_file_removed_to_lockers(queue_t* locks_queue)
{
    NRET_IF(!locks_queue);

    server_packet_op_t op = OP_OK;
    int error = EIDRM;

    FOREACH_Q(locks_queue) {
        int client_fd = *(VALUE_IT_Q(int*));

        if(writen(client_fd, &op, sizeof(op)))
            writen(client_fd, &error, sizeof(error));
    }
}

// Handles the files replaced by the file system, used to notify the lock queue(the clients waiting for the locks) of each file that the files got removed,
// logs the replacement action and if the send_back flag is set the data is sent back to the client making the request
// Must be called regardless of your needs if the replacement policy is called because of memory cleanup
static int on_files_replaced(int client, bool_t are_replaced, bool_t send_back, linked_list_t* repl_list)
{
    if(!are_replaced || !repl_list)
    {
        size_t zero = 0;
        if(send_back)
        {
            writen(client, &zero, sizeof(zero));
        }

        return 1;
    }
    
    size_t num_files_replaced = ll_count(repl_list);
    int writen_res = 1;
    if(send_back)
    {
        if(writen_res)
            writen_res = writen(client, &num_files_replaced, sizeof(num_files_replaced));
    }

    char* files_removed_str;
    size_t char_needed = num_files_replaced * (MAX_PATHNAME_API_LENGTH + 1);
    CHECK_FATAL_EQ(files_removed_str, malloc(char_needed), NULL, NO_MEM_FATAL);

    int data_cleaned = 0;
    int files_rem_str_index = 0;
    FOREACH_LL(repl_list) {
        replaced_file_t* file = VALUE_IT_LL(replaced_file_t*);
        size_t file_size = replfile_get_data_size(file);
        data_cleaned += file_size;
        notify_file_removed_to_lockers(replfile_get_locks_queue(file));

        char* pathname = replfile_get_pathname(file);
        if(send_back)
        {
            size_t pathname_len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
            if(writen_res && (writen_res = writen_string(client, pathname, pathname_len)))
            {
                if(writen_res && (writen_res = writen(client, &file_size, sizeof(size_t))) && file_size > 0)
                    writen_res = writen(client, replfile_get_data(file), file_size);
            }
        }

        char* file_path = replfile_get_pathname(file);
        bool_t is_last = node_get_next(CURR_IT_LL) == NULL;
        char* next_token = is_last ? "%s\0" : "%s,\0";
        snprintf(files_removed_str + files_rem_str_index, char_needed, next_token, file_path);
        files_rem_str_index += strnlen(file_path, MAX_PATHNAME_API_LENGTH) + !is_last;
    }

    ll_empty(repl_list, FREE_FUNC(free_replfile));
    free(repl_list);
    
    // enough length to log the entire formatted text
    size_t log_len = 150 + files_rem_str_index;
    LOG_EVENT("OP_REPLACEMENT replaced %zu files and cleaned %d bytes. Files: [%s] [Success]", log_len, num_files_replaced, data_cleaned, files_removed_str);
    free(files_removed_str);
    return 1;
}

int handle_open_file_req(int sender)
{
    int result = 0, error;
    int flags;
    CHECK_READ(error, &flags, sizeof(int), sender, "OP_OPEN_FILE");
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(error, pathname, sender, "OP_OPEN_FILE");

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

    // if result == 0 => lock given/file opened | result == -1 => lock enqueued, no response yet
    if(result == 0)
    {
        server_packet_op_t res_op = OP_OK;
        writen(sender, &res_op, sizeof(server_packet_op_t));
    }
    
    return result;
}

int handle_write_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_WRITE_FILE");
    bool_t send_back;
    CHECK_READ(read_result, &send_back, sizeof(bool_t), sender, "OP_WRITE_FILE");

    size_t data_size;
    CHECK_READ(read_result, &data_size, sizeof(data_size), sender, "OP_WRITE_FILE");
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_READ(read_result, data, data_size, sender, "OP_WRITE_FILE");
    }
    
    server_packet_op_t res_op = OP_OK;

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

    int mem_missing = 0;
    linked_list_t* replaced_files = NULL;

    if(data_size > 0)
    {
        if(is_size_too_big(fs, data_size))
        {
            release_read_lock_file(file);
            release_write_lock_fs(fs);
            free(data);
            return return_response_error("OP_WRITE_FILE", pathname, sender, EFBIG);
        }
        release_read_lock_file(file);

        mem_missing = is_size_available(fs, data_size);

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
    }
    else
    {
        release_read_lock_file(file);
        acquire_write_lock_file(file);
        RESET_FILE_WRITEMODE(file);
        release_write_lock_file(file);
    }

    release_write_lock_fs(fs);

    LOG_EVENT("OP_WRITE_FILE run by %d on file %s data written %zu [Success]", -1, sender, pathname, data_size);
    int error_write;
    if((error_write = writen(sender, &res_op, sizeof(server_packet_op_t))))
    {
        if(data_size == 0 && send_back)
            writen(sender, &data_size, sizeof(size_t));
    }
    if(data_size > 0)
        on_files_replaced(sender, mem_missing > 0, error_write ? send_back : FALSE, replaced_files);
    return 0;
}

int handle_append_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_APPEND_FILE");
    bool_t send_back;
    CHECK_READ(read_result, &send_back, sizeof(bool_t), sender, "OP_APPEND_FILE");

    size_t data_size;
    CHECK_READ(read_result, &data_size, sizeof(data_size), sender, "OP_APPEND_FILE");
    void* data = NULL;
    if(data_size > 0)
    {
        CHECK_FATAL_EQ(data, malloc(data_size), NULL, NO_MEM_FATAL);
        CHECK_READ(read_result, data, data_size, sender, "OP_APPEND_FILE");
    }

    server_packet_op_t res_op = OP_OK;

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
    if(!file_is_opened_by(file, sender))
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_APPEND_FILE", pathname, sender, EPERM);
    }

    int lock_owner = file_get_lock_owner(file);
    if(lock_owner != -1 && lock_owner != sender)
    {
        release_read_lock_file(file);
        release_write_lock_fs(fs);
        free(data);
        return return_response_error("OP_APPEND_FILE", pathname, sender, EACCES);
    }

    int mem_missing = 0;
    linked_list_t* replaced_files = NULL;

    if(data_size > 0)
    {
        if(is_size_too_big(fs, data_size))
        {
            release_read_lock_file(file);
            release_write_lock_fs(fs);
            free(data);
            return return_response_error("OP_APPEND_FILE", pathname, sender, EFBIG);
        }
        release_read_lock_file(file);

        mem_missing = is_size_available(fs, data_size);

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
    }
    else
    {
        release_read_lock_file(file);
        acquire_write_lock_file(file);
        RESET_FILE_WRITEMODE(file);
        notify_used_file(file);
        release_write_lock_file(file);
    }

    release_write_lock_fs(fs);

    LOG_EVENT("OP_APPEND_FILE run by %d on file %s data written %zu [Success]", -1, sender, pathname, data_size);
    int error_write;
    if((error_write = writen(sender, &res_op, sizeof(server_packet_op_t))))
    {
        if(data_size == 0 && send_back)
            writen(sender, &data_size, sizeof(size_t));
    }
    if(data_size > 0)
        on_files_replaced(sender, mem_missing > 0, error_write ? send_back : FALSE, replaced_files);
    return 0;
}

int handle_read_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_READ_FILE");

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
    server_packet_op_t res_op = OP_OK;
    if(writen(sender, &res_op, sizeof(server_packet_op_t)))
    {
        if(writen(sender, &content_size, sizeof(size_t)))
        {
            if(content_size > 0)
            {
                char* read_data = file_get_data(file);
                writen(sender, &read_data, content_size);
            }
        }
    }

    release_read_lock_file(file);

    acquire_write_lock_file(file);
    notify_used_file(file);
    release_write_lock_file(file);
    
    release_read_lock_fs(fs);

    LOG_EVENT("OP_READ_FILE run by %d on file %s data read %zu [Success]", -1, sender, pathname, content_size);
    return 0;
}

int handle_nread_files_req(int sender)
{
    int read_result;
    int n_to_read;
    CHECK_READ(read_result, &n_to_read, sizeof(int), sender, "OP_NREAD_FILE");
    bool_t read_all = n_to_read <= 0;

    server_packet_op_t res_op = OP_OK;
    file_system_t* fs = get_fs();

    acquire_read_lock_fs(fs);
    file_stored_t** files = get_files_stored(fs);
    size_t fs_file_count = get_file_count_fs(fs);
    size_t files_readed = read_all ? fs_file_count : MIN(n_to_read, fs_file_count);

    if(writen(sender, &res_op, sizeof(server_packet_op_t)) == -1)
    {
        release_read_lock_fs(fs);
        return 0;
    }
    if(writen(sender, &files_readed, sizeof(size_t)) == -1)
    {
        release_read_lock_fs(fs);
        return 0;
    }

    // match the array boundaries (example: 4 files means -> [0, 3])
    int i = files_readed - 1;
    int data_read = 0;
    while(i >= 0)
    {
        file_stored_t* curr_file = files[i];
        acquire_read_lock_file(curr_file);
        char* pathname = file_get_pathname(curr_file);
        size_t pathname_len = strnlen(pathname, MAX_PATHNAME_API_LENGTH);
        if(writen_string(sender, pathname, pathname_len) == -1)
        {
            release_read_lock_file(curr_file);
            break;
        }

        size_t curr_size = file_get_size(curr_file);
        if(writen(sender, &curr_size, sizeof(curr_size)) == -1)
        {
            release_read_lock_file(curr_file);
            break;
        }

        if(curr_size > 0 && writen(sender, file_get_data(curr_file), curr_size) == -1)
        {
            release_read_lock_file(curr_file);
            break;
        }

        release_read_lock_file(curr_file);

        data_read += curr_size;
        --i;
    }
    release_read_lock_fs(fs);

    free(files);
    LOG_EVENT("OP_READN_FILE run by %d file readed %zu data read %d [Success]", -1, sender, files_readed, data_read);
    return 0;
}

int handle_remove_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_REMOVE_FILE");

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
    server_packet_op_t res_op = OP_OK;
    writen(sender, &res_op, sizeof(server_packet_op_t));
    return 0;
}

int handle_lock_file_req(int sender)
{
    int result = 0;
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_LOCK_FILE");

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
    if(result == 0)
    {
        server_packet_op_t res_op = OP_OK;
        writen(sender, &res_op, sizeof(server_packet_op_t));
    }
    return result;
}

int handle_unlock_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_UNLOCK_FILE");

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
        return return_response_error("OP_UNLOCK_FILE", pathname, sender, EACCES);
    }

    int new_owner = file_dequeue_lock(file);
    file_set_lock_owner(file, new_owner);
    notify_given_lock(new_owner);

    notify_used_file(file);
    release_write_lock_file(file);
    release_write_lock_fs(fs);

    LOG_EVENT("OP_UNLOCK_FILE run by %d on file %s [Success]", -1, sender, pathname);
    server_packet_op_t res_op = OP_OK;
    writen(sender, &res_op, sizeof(server_packet_op_t));
    return 0;
}

int handle_close_file_req(int sender)
{
    int read_result;
    char pathname[MAX_PATHNAME_API_LENGTH + 1];
    CHECK_READ_PATH(read_result, pathname, sender, "OP_CLOSE_FILE");

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

    server_packet_op_t res_op = OP_OK;
    writen(sender, &res_op, sizeof(server_packet_op_t));
    return 0;
}