#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <dirent.h>

#include "client_params.h"
#include "file_storage_api.h"
#include "utils.h"

void print_help();

void send_filenames();
void send_file_inside_dirs();
void read_filenames();
void read_n_files();
void lock_filenames();
void unlock_filenames();
void remove_filenames();

int read_file(const char* filename);
int send_file(char* pathname, char* dirname);
int lock_file(const char* filename);
int unlock_file(const char* filename);
int remove_file(const char* pathname);

int main(int argc, char **argv)
{
    init_client_params(&g_params);

    int error =read_args_client_params(argc, argv, g_params);
    if(error == -1)
    {
        free_client_params(g_params);
        PRINT_FATAL(EINVAL, "Couldn't read the config commands correctly!");
        return EXIT_FAILURE;
    }
    if(client_is_print_help(g_params))
    {
        print_help();
        free_client_params(g_params);
        return 0;
    }

    print_params(g_params);
    if(check_prerequisited(g_params) == -1)
    {
        free_client_params(g_params);
        return EXIT_FAILURE;
    }

    struct timespec timeout = { time(0) + 5, 0 };
    int interval = 400;
    char* socket_name = client_get_socket_name(g_params);
    int connected = openConnection(socket_name, interval, timeout);
    if(connected == -1)
    {
        free_client_params(g_params);
        PRINT_FATAL(errno, "Couldn't connect to host!");
        return 0;
    }

    PRINT_INFO("Connected to server!");

    send_filenames();
    send_file_inside_dirs();

    lock_filenames();
    remove_filenames();

    read_filenames();
    read_n_files();

    unlock_filenames();

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

void lock_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_lockable(g_params));
    while(curr)
    {
        char* pathname = node_get_value(curr);
        pathname = get_filename_from_path(pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH), NULL);
        lock_file(pathname);

        curr = node_get_next(curr);
    }
}

void unlock_filenames()
{   
    node_t* curr = ll_get_head_node(client_file_list_unlockable(g_params));
    while(curr)
    {
        char* pathname = node_get_value(curr);
        pathname = get_filename_from_path(pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH), NULL);
        unlock_file(pathname);
        
        curr = node_get_next(curr);
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
    result = API_CALL(closeFile(filename));

    // may be useful in future this result var
    return has_locked;
}

int unlock_file(const char* filename)
{
    int result = API_CALL(openFile(filename, O_LOCK));
    if(result == -1)
    {
        return -1;
    }

    int has_unlocked = API_CALL(unlockFile(filename));
    result = API_CALL(closeFile(filename));

    // may be useful in the future this var
    return has_unlocked;
}

int send_files_inside_dir_rec(const char* dirname, bool_t send_all, int* remaining)
{
    DIR* d;
    struct dirent *dir;

    CHECK_ERROR_EQ(d, opendir(dirname), NULL, -1, "Recursive send files cannot opendir %s!", dirname);
    char* dirname_replaced_files = client_dirname_replaced_files(g_params);

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
            
            bool_t success = send_file(pathname_file, dirname_replaced_files) != -1;

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

void send_file_inside_dirs()
{
    char* dir = client_dirname_file_sendable(g_params);
    if(!dir)
        return;

    int num = client_dirname_file_sendable_num(g_params);
    send_files_inside_dir_rec(dir, num == 0, &num);
}

void send_filenames()
{
    char* dirname = client_dirname_replaced_files(g_params);
    node_t* curr = ll_get_head_node(client_file_list_sendable(g_params));
    while(curr)
    {
        char* curr_pathname = node_get_value(curr);
        send_file(curr_pathname, dirname);

        curr = node_get_next(curr);
    }
}

int send_file(char* pathname, char* dirname)
{
    char* filename = get_filename_from_path(pathname, strnlen(pathname, MAX_PATHNAME_API_LENGTH), NULL);

    int result = API_CALL(openFile(filename, O_CREATE | O_LOCK));
    if(result == -1)
    {
        return -1;
    }

    int did_write = API_CALL(writeFile(pathname, dirname));
    result = API_CALL(closeFile(filename));

    // use result in the future (?)

    return did_write;
}

int read_file(const char* filename)
{
    int result = API_CALL(openFile(filename, 0));
    if(result == -1)
        return -1;

    void* buffer;
    size_t buffer_size;
    int has_read = API_CALL(readFile(filename, &buffer, &buffer_size));
    result = API_CALL(closeFile(filename));

    if(has_read == -1)
        return -1;

    char* dirname_readed_files = client_dirname_readed_files(g_params);
    if(dirname_readed_files)
    {
        // save file
        size_t dirname_len = strnlen(dirname_readed_files, MAX_PATHNAME_API_LENGTH);
        size_t filename_len = strnlen(filename, MAX_PATHNAME_API_LENGTH);
        char* full_path;
        CHECK_FATAL_EQ(full_path, malloc(sizeof(char) * (dirname_len + filename_len + 1)), NULL, NO_MEM_FATAL);
        if(buildpath(full_path, dirname_readed_files, (char*)filename, dirname_len, filename_len) == -1)
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
        
        free(full_path);
    }

    free(buffer);
    return has_read;
}

void read_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_readable(g_params));
    while(curr)
    {
        char* curr_filename = node_get_value(curr);
        char* filename = get_filename_from_path(curr_filename, strnlen(curr_filename, MAX_PATHNAME_API_LENGTH), NULL);
        read_file(filename);

        curr = node_get_next(curr);
    }
}

void read_n_files()
{
    int n_read = client_num_file_readed(g_params);
    if(n_read >= 0)
        readNFiles(n_read, client_dirname_readed_files(g_params));
}

void remove_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_removable(g_params));
    while(curr)
    {
        char* curr_filename = node_get_value(curr);
        char* filename = get_filename_from_path(curr_filename, strnlen(curr_filename, MAX_PATHNAME_API_LENGTH), NULL);
        remove_file(filename);

        curr = node_get_next(curr);
    }
}

int remove_file(const char* filename)
{
    int result = API_CALL(openFile(filename, O_LOCK));
    if(result == -1)
    {
        return -1;
    }

    int has_removed = API_CALL(removeFile(filename));
    if(has_removed == -1)
    {
        result = API_CALL(closeFile(filename));
    }

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