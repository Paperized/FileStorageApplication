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
void read_files_sent();
void remove_filenames();

void send_file_inside_dirs();

linked_list_t files_sent;

int main(int argc, char **argv)
{
    init_client_params(&g_params);

    int error = read_args_client_params(argc, argv, &g_params);
    if(error != 0)
    {
        printf("Missing params or invalid params!.\n");
        return 0;
    }

    if(g_params.print_help)
    {
        print_help();
        return 0;
    }

    print_params(&g_params);
    if(check_prerequisited(&g_params) == -1)
    {
        free_client_params(&g_params);
        return 0;
    }

    struct timespec t;
    t.tv_sec = 10;
    int connected = openConnection(g_params.server_socket_name, 1, t);
    if(connected != 0)
    {
        printf("couldnt connect to the server.\n");
    }
    else
    {
        printf("connected to server.\n");

        linked_list_t empty = INIT_EMPTY_LL;
        files_sent = empty;

        send_filenames();
        send_file_inside_dirs();

        read_filenames();
        //read_files_sent();

        remove_filenames();

        int closed = closeConnection(g_params.server_socket_name);
        if(closed != 0)
            printf("error during closeConnection.\n");
        else
            printf("connection closed.\n");
    }

    free_client_params(&g_params);
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
                ll_add_tail(&files_sent, dir->d_name);
                *remaining -= 1;
                closeFile(dir->d_name);
                continue;
            }

            ll_add_tail(&files_sent, dir->d_name);
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
    node_t* curr_dir = g_params.dirname_file_sendable.head;
    while(curr_dir != NULL)
    {
        string_int_pair_t* value = curr_dir->value;
        int remaining = value->int_value;
        send_files_inside_dir_rec(value->str_value, value->int_value == 0, &remaining);
    }
}

void send_filenames()
{
    node_t* curr = g_params.file_list_sendable.head;
    while(curr != NULL)
    {
        char* curr_filename = curr->value;
        if(openFile(curr_filename, OP_CREATE) == -1)
        {
            printf("Skipping (Open) %s.\n", curr_filename);
            curr = curr->next;
            continue;
        }

        if(writeFile(curr_filename, NULL) == -1)
        {
            printf("Skipping (Write) %s.\n", curr_filename);
            ll_add_tail(&files_sent, curr_filename);
            closeFile(curr_filename);
            curr = curr->next;
            continue;
        }

        ll_add_tail(&files_sent, curr_filename);

        if(closeFile(curr_filename) == -1)
        {
            printf("Skipping (Close) %s.\n", curr_filename);
        }

        curr = curr->next;
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

    if(g_params.dirname_readed_files != NULL)
    {
        // save file
        char *dirname, *fname;
        extract_dirname_and_filename(filename, &dirname, &fname);

        char* newpath_built = buildpath(g_params.dirname_readed_files, fname, strlen(g_params.dirname_readed_files), strlen(fname));
        if(write_file_util(newpath_built, buffer, buffer_size) == -1)
        {
            printf("Couldnt save file: %s in dir: %s.\n", fname, g_params.dirname_readed_files);
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
    node_t* curr = g_params.file_list_readable.head;
    while(curr != NULL)
    {
        char* curr_filename = curr->value;
        read_file(curr_filename);

        curr = curr->next;
    }
}

void read_files_sent()
{
    int n = g_params.num_file_readed;
    bool_t all = n == 0;

    node_t* curr = files_sent.head;
    while(curr != NULL && (all || n > 0))
    {
        char* curr_filename = curr->value;
        read_file(curr_filename);

        n -= 1;
        curr = curr->next;
    }
}

void remove_filenames()
{
    node_t* curr = g_params.file_list_removable.head;
    while(curr != NULL)
    {
        char* curr_filename = curr->value;
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

        curr = curr->next;
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