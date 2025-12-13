CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: lab9_1 sender receiver

lab9_1: lab9_1.c
	$(CC) $(CFLAGS) lab9_1.c -o lab9_1

sender: sender.c
	$(CC) sender.c -o sender

receiver: receiver.c
	$(CC) receiver.c -o receiver

clean:
	rm -f lab9_1 sender receiver
