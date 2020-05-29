CC = gcc
# CFLAGS = 
LIBS = -lSDL2 -lSDL2_image -lpthread -lm

SERVER_DEPS = linked_list.c UI_library.c

server: server.c
	$(CC) server.c $(SERVER_DEPS) -o server $(LIBS)

client: client.c
	$(CC) client.c UI_library.c -o client $(LIBS)

clean:
	rm -f server
	rm -f client
