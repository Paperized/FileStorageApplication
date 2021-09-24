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

void read_file(const char* filename);
int send_file(char* pathname, char* dirname);
void lock_file(const char* filename);
void unlock_file(const char* filename);

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

    read_filenames();
    read_n_files();

    lock_filenames();
    unlock_filenames();

    remove_filenames();

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
        unlock_file(pathname);
        
        curr = node_get_next(curr);
    }
}

void lock_file(const char* filename)
{
    int result = API_CALL(openFile(filename, 0));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Lock file (Open) %s failed!", ARGS(filename));
        return;
    }

    result = API_CALL(lockFile(filename));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Lock file (Lock) %s failed!", ARGS(filename));
    }

    result = API_CALL(closeFile(filename));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Lock file (Close) %s failed!", ARGS(filename));
    }
}

void unlock_file(const char* filename)
{
    int result = API_CALL(openFile(filename, O_LOCK));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Unlock file (Open) %s failed!", ARGS(filename));
        return;
    }

    result = API_CALL(unlockFile(filename));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Unlock file (Unlock) %s failed!", ARGS(filename));
    }

    result = API_CALL(closeFile(filename));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Write file (Close) %s failed!", ARGS(filename));
    }
}

int send_files_inside_dir_rec(const char* dirname, bool_t send_all, int* remaining)
{
    DIR* d;
    struct dirent *dir;

    CHECK_ERROR_EQ(d, opendir(dirname), NULL, -1, "Recursive send files cannot opendir %s!", dirname);
    char* dirname_replaced_files = client_dirname_replaced_files(g_params);

    while ((dir = readdir(d)) != NULL && (send_all == TRUE || *remaining > 0)) {
        if(dir->d_type != DT_DIR)
        {
            bool_t success = send_file(dir->d_name, dirname_replaced_files) != -1;
            if(success)
                *remaining -= 1;
        }
        else if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0)
        {
            send_files_inside_dir_rec(dir->d_name, send_all, remaining);
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
        char* curr_filename = node_get_value(curr);
        send_file(curr_filename, dirname);

        curr = node_get_next(curr);
    }
}

int send_file(char* pathname, char* dirname)
{
    int result = API_CALL(openFile(pathname, O_CREATE | O_LOCK));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Write file (Open) %s failed!", ARGS(pathname));
        return -1;
    }

    int did_write = API_CALL(writeFile(pathname, dirname));
    if(did_write == -1)
    {
        PRINT_WARNING(errno, "Write file (Write) %s failed!", ARGS(pathname));
    }

    result = API_CALL(closeFile(pathname));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Write file (Close) %s failed!", ARGS(pathname));
    }

    return did_write;
}

void read_file(const char* filename)
{
    int result = API_CALL(openFile(filename, 0));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Read file (Open) %s failed!", ARGS(filename));
        return;
    }

    void* buffer;
    size_t buffer_size;
    int has_read = API_CALL(readFile(filename, &buffer, &buffer_size));
    if(has_read == -1)
    {
        PRINT_WARNING(errno, "Read file (Read) %s failed!", ARGS(filename));
    }

    result = API_CALL(closeFile(filename));
    if(result == -1)
    {
        PRINT_WARNING(errno, "Read file (Close) %s failed!", ARGS(filename));
    }

    if(has_read == -1)
        return;

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
}

void read_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_readable(g_params));
    while(curr)
    {
        char* curr_filename = node_get_value(curr);
        read_file(curr_filename);

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

        int result = API_CALL(openFile(curr_filename, O_LOCK));
        if(result == -1)
        {
            PRINT_WARNING(errno, "Remove file (Open) %s failed!", ARGS(curr_filename));
            curr = node_get_next(curr);
            continue;
        }

        result = API_CALL(removeFile(curr_filename));
        if(result == -1)
        {
            PRINT_WARNING(errno, "Remove file (Remove) %s failed!", ARGS(curr_filename));
            result = API_CALL(closeFile(curr_filename));
            if(result == -1)
            {
                PRINT_WARNING(errno, "Remove file (Close) %s failed!", ARGS(curr_filename));
            }
        }

        curr = node_get_next(curr);
    }
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
            "-l file1[,file2], which files will be removed from the server separated by a comma (if they exists)\n"
            "-u file1[,file2], which files will be removed from the server separated by a comma (if they exists)\n"
            "-p enable log of each operation\n");
}