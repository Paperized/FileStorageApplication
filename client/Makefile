CC = gcc
CFLAGS = -Wall -pedantic -I ./includes -I ../shared_lib

ODIR = ./obj
BDIR = ./bin

LIBS = -pthread

bin/client: obj/file_storage_api.o obj/queue.o obj/packet.o obj/linked_list.o obj/server_api_utils.o obj/utils.o
	$(CC) $(CFLAGS) src/main.c -o $@.out $^ $(LIBS)

obj/file_storage_api.o: src/file_storage_api.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/queue.o: ../shared_lib/queue.c
	$(CC) $(CFLAGS) -c -o $@ $< 

obj/linked_list.o: ../shared_lib/linked_list.c
	$(CC) $(CFLAGS) -c -o $@ $< 

obj/packet.o: ../shared_lib/packet.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/server_api_utils.o: ../shared_lib/server_api_utils.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/utils.o: ../shared_lib/utils.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(ODIR)/* *~ core $(INCDIR)/*~

