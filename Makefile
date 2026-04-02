CC = gcc

NOB_SRC = src_bootstrap/nob.c
NOB_BIN = nob

MODE ?= d

.PHONY: all bootstrap

all: $(NOB_BIN)
	./$(NOB_BIN) -$(MODE)

# Bootstraping nob
$(NOB_BIN):
	$(CC) -o $(NOB_BIN) $(NOB_SRC)

bootstrap: $(NOB_BIN)

