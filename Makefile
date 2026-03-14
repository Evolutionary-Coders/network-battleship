CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
SRC_DIR = src

SERVER_SRC = $(SRC_DIR)/main.c $(SRC_DIR)/game.c $(SRC_DIR)/fifo.c \
             $(SRC_DIR)/ship.c $(SRC_DIR)/http.c $(SRC_DIR)/json.c \
             $(SRC_DIR)/server.c
ATTACKER_SRC = $(SRC_DIR)/attacker.c

all: miniwebserver atacante

miniwebserver: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

atacante: $(ATTACKER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f miniwebserver atacante /tmp/batalha_naval_fifo

.PHONY: all clean
