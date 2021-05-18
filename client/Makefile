CC = gcc
CFLAGS = -Wall -pedantic -I ./includes -I ../shared_lib

ODIR = ./obj
BDIR = ./bin

LIBS = -pthread

bin/client: obj/file_storage_api.o obj/queue.o obj/packet.o obj/linked_list.o obj/server_api_utils.o obj/utils.o obj/client_params.o
	$(CC) $(CFLAGS) -g src/main.c -o $@.out $^ $(LIBS)

obj/client_params.o: src/client_params.c
	$(CC) $(CFLAGS) -g -c -o $@ $<

obj/file_storage_api.o: src/file_storage_api.c
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

