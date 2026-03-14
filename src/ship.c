#include "ship.h"
#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define MOVE_INTERVAL 5
#define BOARD_COLS 10

void ship_run(int ship_id, int initial_col) {
    srand(time(NULL) ^ (getpid() << 16));

    int fd = fifo_open_write();
    if (fd < 0) {
        perror("ship: fifo_open_write");
        _exit(1);
    }

    int col = initial_col;
    char buf[32];

    /* Send initial position */
    int len = snprintf(buf, sizeof(buf), "%d:%d\n", ship_id, col);
    write(fd, buf, len);

    while (1) {
        sleep(MOVE_INTERVAL);

        int direction = (rand() % 2 == 0) ? 1 : -1;
        int new_col = col + direction;

        /* Clamp to board boundaries */
        if (new_col < 0) new_col = 1;
        if (new_col >= BOARD_COLS) new_col = BOARD_COLS - 2;

        if (new_col != col) {
            col = new_col;
            len = snprintf(buf, sizeof(buf), "%d:%d\n", ship_id, col);
            write(fd, buf, len);
        }
    }
}
