# Makefile — автоматическая сборка myls (без автозапуска)

CC      := gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -MMD -MP
LDFLAGS :=
SRC     := myls.c
OBJ     := $(SRC:.c=.o)
DEPS    := $(SRC:.c=.d)
BIN     := myls

.PHONY: all debug clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS := -std=c11 -g -O0 -Wall -Wextra -MMD -MP
debug: clean all

clean:
	$(RM) $(OBJ) $(DEPS) $(BIN)

-include $(DEPS)
