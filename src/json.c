#include "json.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int json_shot_result(char *buf, int buf_size, const Shot *shot) {
    if (strcmp(shot->result, "acerto") == 0) {
        return snprintf(buf, buf_size,
            "{\"resultado\":\"acerto\",\"tipo\":\"%s\",\"pontos\":%d}",
            shot->ship_type,
            strcmp(shot->ship_type, "porta_avioes") == 0 ? 5 :
            strcmp(shot->ship_type, "submarino") == 0 ? 3 : 2);
    }
    return snprintf(buf, buf_size, "{\"resultado\":\"%s\"}", shot->result);
}

int json_status(char *buf, int buf_size, GameState *state) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"linhas\":[");

    int first = 1;
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (!state->ships[i].alive) continue;
        if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"linha\":%d,\"tipo\":\"%s\"}",
            state->ships[i].row, state->ships[i].type);
        first = 0;
    }

    offset += snprintf(buf + offset, buf_size - offset,
        "],\"quantidade\":{\"porta_avioes\":%d,\"submarinos\":%d,\"fragatas\":%d}}",
        game_count_alive(state, "porta_avioes"),
        game_count_alive(state, "submarino"),
        game_count_alive(state, "fragata"));

    return offset;
}

int json_local_state(char *buf, int buf_size, GameState *state) {
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset, "{\"navios\":[");
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        Ship *s = &state->ships[i];
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"id\":%d,\"tipo\":\"%s\",\"linha\":%d,\"coluna\":%d,\"vivo\":%s}",
            s->id, s->type, s->row, s->col,
            s->alive ? "true" : "false");
    }

    offset += snprintf(buf + offset, buf_size - offset, "],\"tiros\":[");
    for (int i = 0; i < state->shot_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        Shot *t = &state->shots[i];
        char time_str[32];
        struct tm *tm_info = localtime(&t->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"linha\":%d,\"coluna\":%d,\"resultado\":\"%s\",\"atacante\":\"%s\",\"horario\":\"%s\"}",
            t->row, t->col, t->result, t->attacker_ip, time_str);
    }

    offset += snprintf(buf + offset, buf_size - offset,
        "],\"pontuacao_sofrida\":%d}", state->score_suffered);

    return offset;
}
