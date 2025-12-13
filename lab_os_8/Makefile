CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = sync_demo

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)