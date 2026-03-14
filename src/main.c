#include "game.h"
#include "fifo.h"
#include "ship.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static GameState game_state;

static void cleanup(int sig) {
    (void)sig;
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (game_state.ships[i].pid > 0)
            kill(game_state.ships[i].pid, SIGTERM);
    }
    fifo_destroy();
    _exit(0);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    game_init(&game_state);

    if (fifo_create() < 0) {
        perror("fifo_create");
        return 1;
    }

    /* Fork ship processes */
    for (int i = 0; i < NUM_SHIPS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            ship_run(game_state.ships[i].id, game_state.ships[i].col);
            /* ship_run never returns */
        }
        game_state.ships[i].pid = pid;
    }

    int fifo_fd = fifo_open_read();
    if (fifo_fd < 0) {
        perror("fifo_open_read");
        return 1;
    }

    int http_fd = server_create_socket();
    if (http_fd < 0) {
        perror("server_create_socket");
        return 1;
    }

    printf("Miniwebserver running on port %d\n", SERVER_PORT);
    server_run(http_fd, fifo_fd, &game_state);

    return 0;
}
