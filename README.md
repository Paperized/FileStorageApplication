# FileStorageApplication
File storage application made in C using unix libraries and sockets

This is a multithread application divided in two client and server, both share a common folder and have the same interface to comunicate with each other.

It uses sockets, every request has a length, a specific ID specified in an enum, and each field serialized in byte (using utils.h functions).

Only the server is multithread, the concurrency is managed with readwrite locks on the filesystem hashtable and in each file.

A makefile is included with the following commands:

```
all, compile-all, compile-server, compile-client, compile-shared_lib
```

There are some tests included called test1, test2 and test3.
