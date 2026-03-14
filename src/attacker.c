#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define MAX_TARGETS 20
#define MAX_TOTAL_SHOTS 100
#define BOARD_COLS 10
#define RESPONSE_BUF 4096

/* Attack statistics */
typedef struct {
    char ip[46];
    int shots_fired;
    int hits;
    int misses;
    int repeated;
    int points;
} TargetStats;

typedef struct {
    int row;
    char type[20];
    int points;
} ShipInfo;

static TargetStats targets[MAX_TARGETS];
static int num_targets = 0;
static int total_shots = 0;

static int http_get(const char *ip, int port, const char *path, char *response, int resp_size) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, ip, &addr.sin_addr);

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    char request[512];
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, ip, port);
    write(fd, request, len);

    int total = 0;
    ssize_t n;
    while ((n = read(fd, response + total, resp_size - total - 1)) > 0)
        total += n;
    response[total] = '\0';

    close(fd);

    /* Skip HTTP headers, find body after \r\n\r\n */
    char *body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(response, body, strlen(body) + 1);
    }

    return 0;
}

static int parse_status(const char *json, ShipInfo *ships, int max_ships) {
    int count = 0;
    const char *p = json;

    /* Find "linhas" array and parse each entry */
    p = strstr(p, "\"linhas\"");
    if (!p) return 0;

    while (count < max_ships) {
        const char *linha = strstr(p, "\"linha\":");
        if (!linha) break;
        linha += 8;

        const char *tipo = strstr(linha, "\"tipo\":\"");
        if (!tipo) break;
        tipo += 8;

        ships[count].row = atoi(linha);

        /* Extract type string */
        const char *end = strchr(tipo, '"');
        if (!end) break;
        int tlen = (int)(end - tipo);
        if (tlen >= (int)sizeof(ships[count].type)) tlen = sizeof(ships[count].type) - 1;
        strncpy(ships[count].type, tipo, tlen);
        ships[count].type[tlen] = '\0';

        /* Assign points */
        if (strcmp(ships[count].type, "porta_avioes") == 0) ships[count].points = 5;
        else if (strcmp(ships[count].type, "submarino") == 0) ships[count].points = 3;
        else ships[count].points = 2;

        count++;
        p = end + 1;
    }

    return count;
}

/* Sort ships by points descending (porta_avioes first) */
static void sort_ships_by_value(ShipInfo *ships, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (ships[j].points > ships[i].points) {
                ShipInfo tmp = ships[i];
                ships[i] = ships[j];
                ships[j] = tmp;
            }
        }
    }
}

static void attack_target(int target_idx) {
    char *ip = targets[target_idx].ip;
    char response[RESPONSE_BUF];

    printf("\n=== Atacando %s ===\n", ip);

    /* Query /status */
    if (http_get(ip, SERVER_PORT, "/status", response, sizeof(response)) < 0) {
        printf("  Erro: nao foi possivel conectar a %s\n", ip);
        return;
    }

    ShipInfo ships[10];
    int ship_count = parse_status(response, ships, 10);
    if (ship_count == 0) {
        printf("  Nenhum navio encontrado em %s\n", ip);
        return;
    }

    sort_ships_by_value(ships, ship_count);

    printf("  Navios detectados: %d\n", ship_count);
    for (int i = 0; i < ship_count; i++)
        printf("    Linha %d: %s (%d pts)\n", ships[i].row, ships[i].type, ships[i].points);

    /* Sweep all columns for each ship row */
    for (int i = 0; i < ship_count && total_shots < MAX_TOTAL_SHOTS; i++) {
        for (int col = 0; col < BOARD_COLS && total_shots < MAX_TOTAL_SHOTS; col++) {
            char path[128];
            snprintf(path, sizeof(path), "/tiro?linha=%d&coluna=%d", ships[i].row, col);

            if (http_get(ip, SERVER_PORT, path, response, sizeof(response)) < 0) {
                printf("  Erro no tiro linha=%d coluna=%d\n", ships[i].row, col);
                continue;
            }

            total_shots++;
            targets[target_idx].shots_fired++;

            if (strstr(response, "\"acerto\"")) {
                targets[target_idx].hits++;
                /* Extract points from response */
                const char *pts = strstr(response, "\"pontos\":");
                if (pts) targets[target_idx].points += atoi(pts + 9);
                printf("  [%d] Linha %d, Coluna %d: ACERTO! (%s)\n",
                    total_shots, ships[i].row, col, ships[i].type);
                break; /* Ship hit, move to next row */
            } else if (strstr(response, "\"repetido\"")) {
                targets[target_idx].repeated++;
            } else {
                targets[target_idx].misses++;
            }
        }
    }
}

static void print_report(void) {
    printf("\n");
    printf("========================================\n");
    printf("         RELATORIO DE ATAQUE\n");
    printf("========================================\n\n");

    int total_hits = 0, total_misses = 0, total_points = 0, total_repeated = 0;

    for (int i = 0; i < num_targets; i++) {
        TargetStats *t = &targets[i];
        printf("Alvo: %s\n", t->ip);
        printf("  Tiros: %d | Acertos: %d | Agua: %d | Repetidos: %d | Pontos: %d\n\n",
            t->shots_fired, t->hits, t->misses, t->repeated, t->points);
        total_hits += t->hits;
        total_misses += t->misses;
        total_points += t->points;
        total_repeated += t->repeated;
    }

    printf("----------------------------------------\n");
    printf("TOTAL\n");
    printf("  Tiros disparados: %d / %d\n", total_shots, MAX_TOTAL_SHOTS);
    printf("  Acertos: %d\n", total_hits);
    printf("  Agua: %d\n", total_misses);
    printf("  Repetidos: %d\n", total_repeated);
    printf("  Pontuacao total: %d\n", total_points);
    printf("========================================\n");
}

static int read_targets(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Erro ao abrir alvos.txt");
        return -1;
    }

    char line[128];
    while (fgets(line, sizeof(line), f) && num_targets < MAX_TARGETS) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        strncpy(targets[num_targets].ip, line, sizeof(targets[num_targets].ip) - 1);
        num_targets++;
    }

    fclose(f);
    return num_targets;
}

int main(void) {
    if (read_targets("alvos.txt") <= 0) {
        fprintf(stderr, "Nenhum alvo encontrado em alvos.txt\n");
        return 1;
    }

    printf("Processo atacante iniciado\n");
    printf("Alvos carregados: %d\n", num_targets);
    printf("Limite de tiros: %d\n", MAX_TOTAL_SHOTS);

    for (int i = 0; i < num_targets && total_shots < MAX_TOTAL_SHOTS; i++)
        attack_target(i);

    print_report();

    return 0;
}
