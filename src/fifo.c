#include "fifo.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int fifo_create(void) {
    unlink(FIFO_PATH);
    return mkfifo(FIFO_PATH, 0666);
}

int fifo_open_read(void) {
    return open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
}

int fifo_open_write(void) {
    return open(FIFO_PATH, O_WRONLY);
}

int fifo_parse_message(const char *msg, int *ship_id, int *col) {
    return (sscanf(msg, "%d:%d", ship_id, col) == 2) ? 0 : -1;
}

void fifo_destroy(void) {
    unlink(FIFO_PATH);
}
