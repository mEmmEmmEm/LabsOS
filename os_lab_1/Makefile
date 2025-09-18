CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

.PHONY: all clean

all: mycat mygrep

# Собираем бинарники напрямую из исходников — .o не остаются
mycat: main.c mycat.c mygrep.c mycat.h mygrep.h
	$(CC) $(CFLAGS) -o $@ main.c mycat.c mygrep.c

mygrep: main.c mycat.c mygrep.c mycat.h mygrep.h
	$(CC) $(CFLAGS) -o $@ main.c mycat.c mygrep.c

# Кросс-платформенная очистка (Windows и Unix)
clean:
	- del /Q mycat.exe mygrep.exe 2> NUL || true
	- rm -f mycat mygrep 2>/dev/null || true
