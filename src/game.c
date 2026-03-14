#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const struct { const char *type; int points; int count; } SHIP_DEFS[] = {
    {"porta_avioes", 5, 1},
    {"submarino",    3, 2},
    {"fragata",      2, 3},
};

void game_init(GameState *state) {
    memset(state, 0, sizeof(*state));
    srand(time(NULL));

    /* Build list of available rows, then shuffle */
    int rows[BOARD_ROWS];
    for (int i = 0; i < BOARD_ROWS; i++) rows[i] = i;
    for (int i = BOARD_ROWS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = rows[i]; rows[i] = rows[j]; rows[j] = tmp;
    }

    int idx = 0;
    for (int d = 0; d < 3; d++) {
        for (int c = 0; c < SHIP_DEFS[d].count; c++) {
            Ship *s = &state->ships[idx];
            s->id = idx;
            s->type = SHIP_DEFS[d].type;
            s->points = SHIP_DEFS[d].points;
            s->row = rows[idx];
            s->col = rand() % BOARD_COLS;
            s->alive = true;
            s->pid = 0;
            idx++;
        }
    }
}

void game_update_ship_col(GameState *state, int ship_id, int col) {
    if (ship_id >= 0 && ship_id < NUM_SHIPS)
        state->ships[ship_id].col = col;
}

Shot *game_process_shot(GameState *state, int row, int col, const char *attacker_ip) {
    /* Check if already attacked */
    for (int i = 0; i < state->shot_count; i++) {
        if (state->shots[i].row == row && state->shots[i].col == col) {
            Shot *s = &state->shots[state->shot_count++];
            s->row = row;
            s->col = col;
            s->result = "repetido";
            s->ship_type = NULL;
            strncpy(s->attacker_ip, attacker_ip, sizeof(s->attacker_ip) - 1);
            return s;
        }
    }

    /* Check if a ship is at this position */
    Shot *s = &state->shots[state->shot_count++];
    s->row = row;
    s->col = col;
    strncpy(s->attacker_ip, attacker_ip, sizeof(s->attacker_ip) - 1);

    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship *ship = &state->ships[i];
        if (ship->alive && ship->row == row && ship->col == col) {
            s->result = "acerto";
            s->ship_type = ship->type;
            state->score_suffered += ship->points;
            ship->alive = false;
            return s;
        }
    }

    s->result = "agua";
    s->ship_type = NULL;
    return s;
}

int game_count_alive(GameState *state, const char *type) {
    int count = 0;
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (state->ships[i].alive && strcmp(state->ships[i].type, type) == 0)
            count++;
    }
    return count;
}
