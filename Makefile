# dropbox-clone/Makefile
CC = gcc
CFLAGS = -Wall -g -pthread
TARGET = server
CLIENT = client

all: $(TARGET) $(CLIENT)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

$(CLIENT): src/client.c
	$(CC) $(CFLAGS) -o $(CLIENT) src/client.c

clean:
	rm -f $(TARGET) $(CLIENT)

.PHONY: all clean