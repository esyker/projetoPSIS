CC = gcc
# CFLAGS =
LIBS = -lSDL2 -lSDL2_image -lpthread -lm -Wall

SERVER_DEPS = linked_list.c UI_library.c list_operations.c board_operations.c score_operations.c fruit_operations.c player_operations.c

server: server.c $(SERVER_DEPS)
	$(CC) server.c $(SERVER_DEPS) -o server $(LIBS)

server_gdb: server.c $(SERVER_DEPS)
	$(CC) -g server.c $(SERVER_DEPS) -o server $(LIBS)

client: client.c
	$(CC) client.c UI_library.c -o client $(LIBS)

clean:
	rm -f server
	rm -f client
