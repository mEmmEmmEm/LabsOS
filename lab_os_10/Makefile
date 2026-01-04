CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread

all: lab_rwlock

lab_rwlock: main.c
	$(CC) $(CFLAGS) main.c -o lab_rwlock

run: lab_rwlock
	./lab_rwlock

clean:
	rm -f lab_rwlock
