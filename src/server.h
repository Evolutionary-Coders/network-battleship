#ifndef SERVER_H
#define SERVER_H

#include "game.h"

#define SERVER_PORT 8080
#define MAX_REQUEST_SIZE 2048

int server_create_socket(void);
void server_run(int http_fd, int fifo_fd, GameState *state);

#endif
