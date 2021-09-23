#ifndef _FILE_STORAGE_API_H
#define _FILE_STORAGE_API_H

#include <stdlib.h>
#include "client_params.h"

#define API_CALL(fn_call) fn_call; \
                            usleep(1000 * client_ms_between_requests(g_params))

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* pathname, int flags);
int readFile(const char* pathname, void** buf, size_t* size);
int readNFiles(int N, const char* dirname);
int writeFile(const char* pathname, const char* dirname);
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);
int closeFile(const char* pathname);
int removeFile(const char* pathname);
int lockFile(const char* pathname);
int unlockFile(const char* pathname);

#endif