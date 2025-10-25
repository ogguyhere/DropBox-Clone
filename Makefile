# dropbox-clone/Makefile
CC = gcc
CFLAGS = -Wall -g -pthread

SRC_DIR = src
TARGET = server
CLIENT = client
TEST = test_queue

all: $(TARGET) $(CLIENT) $(TEST)

$(TARGET): $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_DIR)/main.c

$(CLIENT): $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) -o $(CLIENT) $(SRC_DIR)/client.c

$(TEST): $(SRC_DIR)/test_queue.c $(SRC_DIR)/queue.c $(SRC_DIR)/worker.c
	$(CC) $(CFLAGS) -o $(TEST) $(SRC_DIR)/test_queue.c $(SRC_DIR)/queue.c $(SRC_DIR)/worker.c $(SRC_DIR)/metadata.c -lpthread

clean:
	rm -f $(TARGET) $(CLIENT) $(TEST)

.PHONY: all clean
