#ifndef FIFO_H
#define FIFO_H

#define FIFO_PATH "/tmp/batalha_naval_fifo"
#define FIFO_BUF_SIZE 256

int fifo_create(void);
int fifo_open_read(void);
int fifo_open_write(void);
int fifo_parse_message(const char *msg, int *ship_id, int *col);
void fifo_destroy(void);

#endif
