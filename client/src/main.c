#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include "client_params.h"
#include "file_storage_api.h"
#include "utils.h"

void print_help();
void send_filenames();
void read_file(const char* filename);
void read_filenames();
void read_n_files();
void remove_filenames();

void send_file_inside_dirs();

int main(int argc, char **argv)
{
    init_client_params(&g_params);

    int error = read_args_client_params(argc, argv, g_params);
    if(error != 0)
    {
        printf("Missing params or invalid params!.\n");
        return 0;
    }

    if(client_is_print_help(g_params))
    {
        print_help();
        return 0;
    }

    print_params(g_params);
    if(check_prerequisited(g_params) == -1)
    {
        free_client_params(g_params);
        return 0;
    }

    struct timespec t;
    t.tv_sec = 10;
    char* socket_name = client_get_socket_name(g_params);
    int connected = openConnection(socket_name, 1, t);
    if(connected != 0)
    {
        printf("couldnt connect to the server.\n");
    }
    else
    {
        printf("connected to server.\n");

        send_filenames();
        send_file_inside_dirs();

        read_filenames();
        read_n_files();

        remove_filenames();

        int closed = closeConnection(socket_name);
        if(closed != 0)
            printf("error during closeConnection.\n");
        else
            printf("connection closed.\n");
    }

    free_client_params(g_params);
    return 0;
}

void send_files_inside_dir_rec(const char* dirname, bool_t send_all, int* remaining)
{
    DIR* d;
    struct dirent *dir;

    if ((d = opendir(dirname)) == NULL)
    {
        printf("Dir %s couldnt be opened.\n", dirname);
        return;
    }

    while ((dir = readdir(d)) != NULL && (send_all == TRUE || *remaining > 0)) {
        if(dir->d_type != DT_DIR)
        {
            if(openFile(dir->d_name, OP_LOCK) == -1)
            {
                printf("Skipping (Open) %s.\n", dir->d_name);
                continue;
            }

            if(writeFile(dir->d_name, NULL) == -1)
            {
                printf("Skipping (Write) %s.\n", dir->d_name);
                *remaining -= 1;
                closeFile(dir->d_name);
                continue;
            }

            *remaining -= 1;

            if(closeFile(dir->d_name) == -1)
            {
                printf("Skipping (Close) %s.\n", dir->d_name);
                continue;
            }
        }
        else if(strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0)
        {
            send_files_inside_dir_rec(dir->d_name, send_all, remaining);
        }
    }

    closedir(d);
}

void send_file_inside_dirs()
{
    node_t* curr_dir = ll_get_head_node(client_dirname_file_sendable(g_params));
    while(curr_dir != NULL)
    {
        string_int_pair_t* value = node_get_value(curr_dir);
        int remaining = pair_get_int(value);
        send_files_inside_dir_rec(pair_get_str(value), remaining == 0, &remaining);
    }
}

void send_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_sendable(g_params));
    while(curr != NULL)
    {
        char* curr_filename = node_get_value(curr);
        if(openFile(curr_filename, OP_CREATE) == -1)
        {
            printf("Skipping (Open) %s.\n", curr_filename);
            curr = node_get_next(curr);
            continue;
        }

        if(writeFile(curr_filename, NULL) == -1)
        {
            printf("Skipping (Write) %s.\n", curr_filename);
            closeFile(curr_filename);
            curr = node_get_next(curr);
            continue;
        }

        if(closeFile(curr_filename) == -1)
        {
            printf("Skipping (Close) %s.\n", curr_filename);
        }

        curr = node_get_next(curr);
    }
}

void read_file(const char* filename)
{
    if(openFile(filename, OP_LOCK) == -1)
    {
        printf("Skipping %s.\n", filename);
        return;
    }

    void* buffer;
    size_t buffer_size;
    if(readFile(filename, &buffer, &buffer_size) == -1)
    {
        printf("Skipping %s.\n", filename);
        closeFile(filename);
        return;
    }

    char* dirname_readed_files = client_dirname_readed_files(g_params);
    if(dirname_readed_files != NULL)
    {
        // save file
        char *dirname, *fname;
        extract_dirname_and_filename(filename, &dirname, &fname);

        char* newpath_built = buildpath(dirname_readed_files, fname, strlen(dirname_readed_files), strlen(fname));
        if(write_file_util(newpath_built, buffer, buffer_size) == -1)
        {
            printf("Couldnt save file: %s in dir: %s.\n", fname, dirname_readed_files);
        }
        
        free(newpath_built);
    }

    free(buffer);

    if(closeFile(filename) == -1)
    {
        printf("Skipping %s.\n", filename);
    }
}

void read_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_readable(g_params));
    while(curr != NULL)
    {
        char* curr_filename = node_get_value(curr);
        read_file(curr_filename);

        curr = node_get_next(curr);
    }
}

void read_n_files()
{
    readNFiles(client_num_file_readed(g_params), client_dirname_readed_files(g_params));
}

void remove_filenames()
{
    node_t* curr = ll_get_head_node(client_file_list_removable(g_params));
    while(curr != NULL)
    {
        char* curr_filename = node_get_value(curr);
        if(openFile(curr_filename, OP_LOCK) == -1)
        {
            printf("Skipping %s.\n", curr_filename);
            continue;
        }

        if(removeFile(curr_filename) == -1)
        {
            printf("Skipping %s.\n", curr_filename);
            closeFile(curr_filename);
            continue;
        }

        curr = node_get_next(curr);
    }
}

void print_help()
{
    // hf:w:W:r:R:d:l:u:c:p
    printf("-h print all commands available\n");
    printf("-f pathname, socket file pathname to the server\n");
    printf("-w dirname[,n=0], dirname in which N files will be wrote to the server (n=0 means all)\n");
    printf("-W file1[,file2], which files will be wrote to the server separated by a comma\n");
    printf("-r file1[,file2], which files will be readed to the server separated by a comma\n");
    printf("-R [n=0], read N numbers of files stored inside the server (n=0 means all)\n");
    printf("-d dirname, dirname in which every file reader with -r -R flags will be saved, can only be used with those flags.\n");
    printf("-t time, time between every request-response from the server\n");
    printf("-c file1[,file2], which files will be removed from the server separated by a comma (if they exists)\n");
    printf("-p enable log of each operation\n");
}