CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread

all: lab_condvar

lab_condvar: main.c
	$(CC) $(CFLAGS) main.c -o lab_condvar

run: lab_condvar
	./lab_condvar

clean:
	rm -f lab_condvar
