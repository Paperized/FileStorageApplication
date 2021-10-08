CC = gcc
CFLAGS = -Wall -pedantic

ODIR = ./obj
BDIR = ./bin

LIBS = -pthread

CDIRNAME = client
SDIRNAME = server

CDIR = ./$(CDIRNAME)
SDIR = ./$(SDIRNAME)
LDIR = ./shared_lib
BDIRNAME = bin

CFLAGS_SERVER = $(CFLAGS) -I $(SDIR)/includes -I $(LDIR)/includes
CFLAGS_CLIENT = $(CFLAGS) -I $(CDIR)/includes -I $(LDIR)/includes
CFLAGS_LIB = $(CFLAGS) -I $(LDIR)/includes

SCRIPTDIR = ./script
EXAMPLE_CONFIG_NAME = example_config.txt
DEFAULT_SOCKETNAME = my_socket.sk

all: clean-shared_lib compile-shared_lib clean-server compile-server clean-client compile-client

compile-all: compile-shared_lib compile-client compile-server
compile-server: $(SDIR)/bin/server
compile-client: $(CDIR)/bin/client
compile-shared_lib: $(LDIR)/bin/shared_lib

$(SDIR)/bin/server: $(SDIR)/obj/config_params.o $(SDIR)/obj/server.o $(SDIR)/obj/handle_client.o $(SDIR)/obj/file_stored.o $(SDIR)/obj/file_system.o $(SDIR)/obj/logging.o $(SDIR)/obj/replacement_policy.o $(LDIR)/bin/shared_lib.a
	$(CC) $(CFLAGS_SERVER) -g $(SDIR)/src/main.c -o $@.out $^ $(LIBS)
	test -f $(BDIR)/$(EXAMPLE_CONFIG_NAME) || $(MAKE) generate-example-config

$(SDIR)/obj/config_params.o: $(SDIR)/src/config_params.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/server.o: $(SDIR)/src/server.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/handle_client.o: $(SDIR)/src/handle_client.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/logging.o: $(SDIR)/src/logging.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/replacement_policy.o: $(SDIR)/src/replacement_policy.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/file_system.o: $(SDIR)/src/file_system.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<

$(SDIR)/obj/file_stored.o: $(SDIR)/src/file_stored.c
	$(CC) $(CFLAGS_SERVER) -g -c -o $@ $<


$(CDIR)/bin/client: $(CDIR)/obj/client_params.o $(CDIR)/obj/file_storage_api.o $(LDIR)/bin/shared_lib.a
	$(CC) $(CFLAGS_CLIENT) -g $(CDIR)/src/main.c -o $@.out $^ $(LIBS)

$(CDIR)/obj/file_storage_api.o: $(CDIR)/src/file_storage_api.c
	$(CC) $(CFLAGS_CLIENT) -g -c -o $@ $<

$(CDIR)/obj/client_params.o: $(CDIR)/src/client_params.c
	$(CC) $(CFLAGS_CLIENT) -g -c -o $@ $<


$(LDIR)/bin/shared_lib: $(LDIR)/obj/utils.o $(LDIR)/obj/icl_hash.o $(LDIR)/obj/packet.o $(LDIR)/obj/linked_list.o $(LDIR)/obj/queue.o $(LDIR)/obj/network_file.o
	ar rcs $@.a $^

$(LDIR)/obj/queue.o: $(LDIR)/src/queue.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $< 

$(LDIR)/obj/linked_list.o: $(LDIR)/src/linked_list.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $< 

$(LDIR)/obj/packet.o: $(LDIR)/src/packet.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $<

$(LDIR)/obj/icl_hash.o: $(LDIR)/src/icl_hash.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $<

$(LDIR)/obj/network_file.o: $(LDIR)/src/network_file.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $< 

$(LDIR)/obj/utils.o: $(LDIR)/src/utils.c
	$(CC) $(CFLAGS_LIB) -g -c -o $@ $<

cleanall: clean-client clean-server clean-shared_lib

clean-client:
	rm -f $(CDIR)/obj/* *~ core $(INCDIR)/*~
clean-server: 
	rm -f $(SDIR)/obj/* *~ core $(INCDIR)/*~
clean-shared_lib: 
	rm -f $(LDIR)/obj/* *~ core $(INCDIR)/*~

define CONFIG_TEMPLATE
SERVER_SOCKET_NAME=<path of socket file (es. ./my_socket.sk)>
SERVER_THREAD_WORKERS=<num of workers (es. 4)>
SERVER_BYTE_STORAGE_AVAILABLE=<server size can be in B, KB, MB, GB, TB (es. 100MB)>
SERVER_MAX_FILES_NUM=<max num of files (es. 50)>
POLICY_NAME=<policy of replacement can be FIFO, LFU, LRU (es. LRU)>
SERVER_BACKLOG_NUM=<max number of socket in queue for connection (es. 10)>
SERVER_LOG_NAME=<path of log file (es. ./logs.log)>
endef

export CONFIG_TEMPLATE
generate-example-config:
	@echo "$$CONFIG_TEMPLATE" > $(SDIR)/$(BDIRNAME)/$(EXAMPLE_CONFIG_NAME)

test1:
	$(MAKE) all && chmod +x $(SCRIPTDIR)/test1.sh && $(SCRIPTDIR)/test1.sh
test2: 
	$(MAKE) all && chmod +x $(SCRIPTDIR)/test2.sh && $(SCRIPTDIR)/test2.sh
test3: 
	$(MAKE) all && chmod +x $(SCRIPTDIR)/test3.sh && $(SCRIPTDIR)/test3.sh