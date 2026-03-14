#ifndef JSON_H
#define JSON_H

#include "game.h"

int json_shot_result(char *buf, int buf_size, const Shot *shot);
int json_status(char *buf, int buf_size, GameState *state);
int json_local_state(char *buf, int buf_size, GameState *state);

#endif
