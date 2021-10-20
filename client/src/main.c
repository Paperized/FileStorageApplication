#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <dirent.h>

#include "client_params.h"
#include "utils.h"
#include "file_storage_api.h"

// Print --help for this program
void print_help();

// Send all files contained inside a queue
void send_files(queue_t* files);

// Send N files from a dirname
void send_folder_files(pair_int_str_t* pair);

// Read all files contained inside a queue
void read_files(queue_t* files);

// Read N files
void read_n_files(long n);

// Lock all files contained inside a queue
void lock_files(queue_t* files);

// Unlock all files contained inside a queue
void unlock_files(queue_t* files);

// Remove all files contained inside a queue
void remove_files(queue_t* files);

// Read file by pathname
int read_file(const char* pathname);

// Write file by pathname
int send_file(char* pathname);

// Lock file by pathname
int lock_file(const char* filename);

// Unlock file by pathname
int unlock_file(const char* filename);

// Remove file by pathname
int remove_file(const char* pathname);

// Current ms to wait between each request
long current_ms_between_reqs = 0;
// Current folder where to save readed files
char* current_save_folder = NULL;
// Current folder where to save replaced files
char* current_save_repl_folder = NULL;

// Utility macro to make an api request and wait for sleep timer
#define API_CALL(fn_call) fn_call; \
                            if(current_ms_between_reqs > 0) \
                                usleep(1000 * current_ms_between_reqs)

int main(int argc, char **argv)
{
    init_client_params(&g_params);

    int error = read_args_client_params(argc, argv, g_params);
    if(error == -1)
    {
        free_client_params(g_params);
        PRINT_FATAL(EINVAL, "Couldn't read the config commands correctly!");
        return EXIT_FAILURE;
    }

    if(g_params->print_help)
    {
        print_help();
        free_client_params(g_params);
        return 0;
    }

    if(check_prerequisited(g_params) == -1)
    {
        free_client_params(g_params);
        return EXIT_FAILURE;
    }

    struct timespec timeout = { time(0) + 5, 0 };
    int interval = 400;
    const char* socket_name = g_params->server_socket_name;
    int connected = openConnection(socket_name, interval, timeout);
    if(connected == -1)
    {
        free_client_params(g_params);
        PRINT_FATAL(errno, "Couldn't connect to host!");
        return 0;
    }

    PRINT_INFO("Connected to server!");

    queue_t* ops = g_params->api_operations;

    FOREACH_Q(ops)
    {
        long n = -1;
        api_option_t* curr_opt = VALUE_IT_Q(api_option_t*);
        switch (curr_opt->op)
        {
        case 't':
            current_ms_between_reqs = *((long*)&curr_opt->args);
            break;
        case 'd':
            current_save_folder = curr_opt->args;
            break;
        case 'D':
            current_save_repl_folder = curr_opt->args;
            break;

        case 'w':
            send_folder_files(curr_opt->args);
            break;
        case 'W':
            send_files(curr_opt->args);
            break;
        case 'r':
            read_files(curr_opt->args);
            break;
        case 'l':
            lock_files(curr_opt->args);
            break;
        case 'u':
            unlock_files(curr_opt->args);
            break;
        case 'c':
            remove_files(curr_opt->args);
            break;
        
        case 'R':
            n = *((long*)&curr_opt->args);
            read_n_files(n);
            break;
        default:
            break;
        }
    }

    int closed = closeConnection(socket_name);
    if(closed == -1)
    {
        PRINT_ERROR(errno, "Problems during close connection!");
    }
    else
    {
        PRINT_INFO("Disconnected from server!");
    }

    free_client_params(g_params);
    return 0;
}

void lock_files(queue_t* files)
{
    FOREACH_Q(files)
    {
        char* pathname = VALUE_IT_Q(char*);
        lock_file(pathname);
    }
}

void unlock_files(queue_t* files)
{   
    FOREACH_Q(files)
    {
        char* pathname = VALUE_IT_Q(char*);
        unlock_file(pathname);
    }
}

int lock_file(const char* filename)
{
    int result = API_CALL(openFile(filename, 0));
    if(result == -1)
    {
        return -1;
    }

    int has_locked = API_CALL(lockFile(filename));

    if(has_locked == -1)
    {
        API_CALL(closeFile(filename));
    }

    // may be useful in future this result var
    return has_locked;
}

int unlock_file(const char* filename)
{
    int has_unlocked = API_CALL(unlockFile(filename));
    if(has_unlocked != -1)
    {
        API_CALL(closeFile(filename));
    }

    // may be useful in the future this var
    return has_unlocked;
}

int send_files_inside_dir_rec(const char* dirname, bool_t send_all, int* remaining)
{
    DIR* d;
    struct dirent *dir;

    CHECK_ERROR_EQ(d, opendir(dirname), NULL, -1, "Recursive send files cannot opendir %s!", dirname);

    char pathname_file[MAX_PATHNAME_API_LENGTH + 1];
    size_t dir_len = strnlen(dirname, MAX_PATHNAME_API_LENGTH);

    while ((dir = readdir(d)) != NULL && (send_all == TRUE || *remaining > 0)) {
        if(dir->d_type != DT_DIR)
        {
            size_t file_len = strnlen(dir->d_name, MAX_PATHNAME_API_LENGTH);
            if(buildpath(pathname_file, dirname, dir->d_name, dir_len, file_len) == -1)
            {
                PRINT_ERROR(errno, "Write File %s exceeded max path length (%zu)!", dir->d_name, dir_len + file_len + 1);
                continue;
            }
            
            bool_t success = send_file(pathname_file) != -1;

            if(success)
                *remaining -= 1;
        }
        else if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0)
        {
            // we reuse pathname_file also for directories
            size_t file_len = strnlen(dir->d_name, MAX_PATHNAME_API_LENGTH);
            if(buildpath(pathname_file, dirname, dir->d_name, dir_len, file_len) == -1)
            {
                PRINT_ERROR(errno, "Write File %s folder exceeded max path length (%zu)!", dir->d_name, dir_len + file_len + 1);
                continue;
            }
            send_files_inside_dir_rec(pathname_file, send_all, remaining);
        }
    }

    closedir(d);
    return 1;
}

void send_folder_files(pair_int_str_t* pair)
{
    int n = pair->num;
    send_files_inside_dir_rec(pair->str, n == 0, &n);
}

void send_files(queue_t* files)
{
    FOREACH_Q(files)
    {
        char* pathname = VALUE_IT_Q(char*);
        send_file(pathname);
    }
}

int send_file(char* pathname)
{
    int result = API_CALL(openFile(pathname, O_CREATE | O_LOCK));
    if(result == -1)
    {
        return -1;
    }

    int did_write = API_CALL(writeFile(pathname, current_save_repl_folder));
    result = API_CALL(closeFile(pathname));

    // use result in the future (?)

    return did_write;
}

int read_file(const char* pathname)
{
    int result = API_CALL(openFile(pathname, 0));
    if(result == -1)
        return -1;

    void* buffer;
    size_t buffer_size;
    int has_read = API_CALL(readFile(pathname, &buffer, &buffer_size));
    result = API_CALL(closeFile(pathname));

    if(has_read == -1)
        return -1;

    if(current_save_folder)
    {
        // save file
        size_t dirname_len = strnlen(current_save_folder, MAX_PATHNAME_API_LENGTH);
        size_t filename_len = 0;
        const char *filename = get_filename_from_path(pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH), &filename_len);
        char full_path[MAX_PATHNAME_API_LENGTH + 1];
        full_path[0] = '\0';
        if(buildpath(full_path, current_save_folder, (char*)filename, dirname_len, filename_len) == -1)
        {
            PRINT_ERROR(errno, "Read file (Saving) %s exceeded max path length (%zu)!", filename, dirname_len + filename_len + 1);
        }
        else
        {
            if(write_file_util(full_path, buffer, buffer_size) == -1)
            {
                PRINT_ERROR(errno, "Read file (Saving) %s failed!", filename);
            }
        }
    }

    free(buffer);
    return has_read;
}

void read_files(queue_t* files)
{
    FOREACH_Q(files)
    {
        char* curr_filename = VALUE_IT_Q(char*);
        read_file(curr_filename);
    }
}

void read_n_files(long n)
{
    if(n >= 0)
        readNFiles(n, current_save_folder);
}

void remove_files(queue_t* files)
{
    FOREACH_Q(files)
    {
        char* curr_filename = VALUE_IT_Q(char*);
        remove_file(curr_filename);
    }
}

int remove_file(const char* pathname)
{
    int has_removed = API_CALL(removeFile(pathname));
    return has_removed;
}

void print_help()
{
    // hf:w:W:r:R:d:l:u:c:p
    PRINT_INFO(
            "-h print all commands available\n"
            "-f pathname, socket file pathname to the server\n"
            "-w dirname[,n=0], dirname in which N files will be wrote to the server (n=0 means all)\n"
            "-W file1[,file2], which files will be wrote to the server separated by a comma\n"
            "-r file1[,file2], which files will be readed to the server separated by a comma\n"
            "-R [n=0], read N numbers of files stored inside the server (n=0 means all)\n"
            "-d dirname, dirname in which every file reader with -r -R flags will be saved, can only be used with those flags.\n"
            "-t time, time between every request-response from the server\n"
            "-c file1[,file2], which files will be removed from the server separated by a comma (if they exists)\n"
            "-l file1[,file2], which files will be locked from the server separated by a comma (if they exists)\n"
            "-u file1[,file2], which files will be unlocked from the server separated by a comma (if they exists)\n"
            "-p enable log of each operation\n");
}