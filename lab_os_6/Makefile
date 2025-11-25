CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
TARGET  = main
SOURCES = main.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean
