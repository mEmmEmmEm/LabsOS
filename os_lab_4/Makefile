CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -O2

all: mychmod
mychmod: main.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f mychmod
