#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#define BOARD_ROWS 10
#define BOARD_COLS 10
#define NUM_SHIPS  6
#define MAX_SHOTS  200

typedef struct {
    int id;
    const char *type;
    int points;
    int row;
    int col;
    bool alive;
    pid_t pid;
} Ship;

typedef struct {
    int row;
    int col;
    const char *result;
    const char *ship_type;
    char attacker_ip[46];
    time_t timestamp;
} Shot;

typedef struct {
    Ship ships[NUM_SHIPS];
    Shot shots[MAX_SHOTS];
    int shot_count;
    int score_suffered;
} GameState;

void game_init(GameState *state);
void game_update_ship_col(GameState *state, int ship_id, int col);
Shot *game_process_shot(GameState *state, int row, int col, const char *attacker_ip);
int game_count_alive(GameState *state, const char *type);

#endif
