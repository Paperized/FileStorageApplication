IDIR = ./includes
CC = gcc
CFLAGS = -Wall -pedantic -I $(IDIR)

ODIR = ./obj
BDIR = ./bin

SERVER_TARGET = bin/server

LIBS = -pthread

_DEPS = server.h config_params.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = config_params.o server.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: ./src/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(SERVER_TARGET): $(OBJ)
	$(CC) $(CFLAGS) ./src/main.c -o ./$(SERVER_TARGET).out $(OBJ) $(LIBS)
	test -f $(BDIR)/config.txt || $(MAKE) force_generate_config

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

