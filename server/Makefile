CC = gcc
CFLAGS = -Wall -pedantic -I ./includes -I ../shared_lib

ODIR = ./obj
BDIR = ./bin

LIBS = -pthread

bin/server: obj/config_params.o obj/server.o obj/queue.o obj/packet.o obj/linked_list.o obj/server_api_utils.o obj/utils.o
	$(CC) $(CFLAGS) -g src/main.c -o $@.out $^ $(LIBS)
	test -f $(BDIR)/config.txt || $(MAKE) force_generate_config

obj/config_params.o: src/config_params.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

obj/server.o: src/server.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

obj/queue.o: ../shared_lib/queue.c
	$(CC) $(CFLAGS) -g -c -o $@ $< 

obj/linked_list.o: ../shared_lib/linked_list.c
	$(CC) $(CFLAGS) -g -c -o $@ $< 

obj/packet.o: ../shared_lib/packet.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

obj/server_api_utils.o: ../shared_lib/server_api_utils.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

obj/utils.o: ../shared_lib/utils.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

clean:
	rm -f $(ODIR)/* *~ core $(INCDIR)/*~


define CONFIG_TEMPLATE
SERVER_SOCKET_NAME=socket_file
SERVER_THREAD_WORKERS=4
SERVER_BYTE_STORAGE_AVAILABLE=23473274
SERVER_MAX_NUM_UPLOADABLE=100
SERVER_MAX_NUM_UPLOADABLE_CLIENT=20
SERVER_BACKLOG_NUM=10

endef

export CONFIG_TEMPLATE
force_generate_config:
	@echo "$$CONFIG_TEMPLATE" > $(BDIR)/config.txt

