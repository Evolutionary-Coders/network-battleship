# Batalha Naval em Rede — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a distributed battleship game in C with ship processes, a miniwebserver, an HTML visualization page, and an attacker process.

**Architecture:** Ship processes (fork'd) communicate positions via a shared FIFO. The miniwebserver uses select() to multiplex FIFO reads and HTTP socket. Remote teams attack via HTTP endpoints. An HTML page polls for state updates.

**Tech Stack:** C (gcc), POSIX APIs (fork, mkfifo, select, sockets, kill), HTML + minimal JS for polling.

**Conventions:** Code in English (variables, functions, structs, comments). Portuguese only for spec-defined routes (`/tiro`, `/status`, `/estado_local`), JSON fields (`resultado`, `linha`, `coluna`, `tipo`, `pontos`, etc.), and `alvos.txt`. Commit messages in Portuguese.

---

## File Structure

```
batalha_naval/
├── Makefile                    # Builds miniwebserver and atacante binaries
├── index.html                  # Board visualization page
├── alvos.txt                   # Target IPs (one per line)
├── src/
│   ├── game.c / game.h        # Game state: ships, shots, score
│   ├── fifo.c / fifo.h        # FIFO creation, reading, parsing
│   ├── ship.c / ship.h        # Ship process logic (runs after fork)
│   ├── http.c / http.h        # HTTP request parsing and response writing
│   ├── json.c / json.h        # JSON string generation for each endpoint
│   ├── server.c / server.h    # Socket setup, select() loop, request dispatch
│   ├── main.c                 # Entry point: init game, fork ships, start server
│   └── attacker.c             # Entry point: read IPs, query status, fire shots
└── docs/
```

---

## Chunk 1: Foundation — Game State, FIFO, Ship Process

### Task 1: Makefile

**Files:**
- Create: `Makefile`

- [ ] **Step 1: Create Makefile**

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
SRC_DIR = src

SERVER_SRC = $(SRC_DIR)/main.c $(SRC_DIR)/game.c $(SRC_DIR)/fifo.c \
             $(SRC_DIR)/ship.c $(SRC_DIR)/http.c $(SRC_DIR)/json.c \
             $(SRC_DIR)/server.c
ATTACKER_SRC = $(SRC_DIR)/attacker.c

all: miniwebserver atacante

miniwebserver: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

atacante: $(ATTACKER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f miniwebserver atacante /tmp/batalha_naval_fifo

.PHONY: all clean
```

- [ ] **Step 2: Create src/ directory and stub files**

Create empty `.c` and `.h` files so `make` can be tested incrementally. Each `.c` file gets a minimal stub (empty main or empty function). Each `.h` file gets an include guard.

- [ ] **Step 3: Verify build structure**

Run: `make clean && make`
Expected: Compiles (possibly with warnings about empty files) but no errors.

- [ ] **Step 4: Commit**

```bash
git add Makefile src/
git commit -m "estrutura inicial do projeto com Makefile e stubs"
```

---

### Task 2: Game State Module

**Files:**
- Create: `src/game.h`
- Create: `src/game.c`

This module owns all game data: ship definitions, positions, shot history, score.

- [ ] **Step 1: Define game.h**

```c
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <sys/types.h>

#define BOARD_ROWS 10
#define BOARD_COLS 10
#define NUM_SHIPS  6
#define MAX_SHOTS  200

typedef struct {
    int id;
    const char *type;   /* "porta_avioes", "submarino", "fragata" */
    int points;
    int row;
    int col;
    bool alive;
    pid_t pid;
} Ship;

typedef struct {
    int row;
    int col;
    const char *result;  /* "agua", "acerto", "repetido" */
    const char *ship_type;
    char attacker_ip[46];
} Shot;

typedef struct {
    Ship ships[NUM_SHIPS];
    Shot shots[MAX_SHOTS];
    int shot_count;
    int score_suffered;
} GameState;

/* Initialize ships with types/points, assign random rows, starting columns */
void game_init(GameState *state);

/* Update a ship's column (called when FIFO message arrives) */
void game_update_ship_col(GameState *state, int ship_id, int col);

/* Process an incoming shot. Returns pointer to the new Shot entry. */
Shot *game_process_shot(GameState *state, int row, int col, const char *attacker_ip);

/* Count alive ships by type */
int game_count_alive(GameState *state, const char *type);

#endif
```

- [ ] **Step 2: Implement game.c**

```c
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
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/game.c src/game.h
git commit -m "módulo de estado do jogo: navios, tiros e pontuação"
```

---

### Task 3: FIFO Module

**Files:**
- Create: `src/fifo.h`
- Create: `src/fifo.c`

- [ ] **Step 1: Define fifo.h**

```c
#ifndef FIFO_H
#define FIFO_H

#define FIFO_PATH "/tmp/batalha_naval_fifo"
#define FIFO_BUF_SIZE 256

/* Create the FIFO file. Returns 0 on success, -1 on error. */
int fifo_create(void);

/* Open FIFO for reading (non-blocking). Returns fd or -1. */
int fifo_open_read(void);

/* Open FIFO for writing. Returns fd or -1. */
int fifo_open_write(void);

/* Parse a message "id:col\n" from buffer. Returns 0 on success. */
int fifo_parse_message(const char *msg, int *ship_id, int *col);

/* Remove the FIFO file. */
void fifo_destroy(void);

#endif
```

- [ ] **Step 2: Implement fifo.c**

```c
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
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/fifo.c src/fifo.h
git commit -m "módulo FIFO: criação, abertura e parsing de mensagens"
```

---

### Task 4: Ship Process Module

**Files:**
- Create: `src/ship.h`
- Create: `src/ship.c`

- [ ] **Step 1: Define ship.h**

```c
#ifndef SHIP_H
#define SHIP_H

/* Entry point for the ship child process. Never returns. */
void ship_run(int ship_id, int initial_col);

#endif
```

- [ ] **Step 2: Implement ship.c**

```c
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
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/ship.c src/ship.h
git commit -m "módulo navio: movimento aleatório e escrita na FIFO"
```

---

## Chunk 2: HTTP, JSON, Server Loop

### Task 5: HTTP Module

**Files:**
- Create: `src/http.h`
- Create: `src/http.c`

Handles parsing of HTTP GET requests and sending HTTP responses.

- [ ] **Step 1: Define http.h**

```c
#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char method[8];
    char path[256];
    char query[512];
    char client_ip[46];
} HttpRequest;

/* Parse a raw HTTP request buffer into HttpRequest. Returns 0 on success. */
int http_parse_request(const char *raw, HttpRequest *req);

/* Extract a query parameter value. Returns NULL if not found. */
const char *http_query_param(const char *query, const char *key, char *value, int value_size);

/* Send an HTTP response with given status, content type, and body. */
void http_send_response(int client_fd, int status, const char *content_type, const char *body);

/* Send the contents of a file as an HTTP response. */
void http_send_file(int client_fd, const char *filepath, const char *content_type);

#endif
```

- [ ] **Step 2: Implement http.c**

```c
#include "http.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int http_parse_request(const char *raw, HttpRequest *req) {
    memset(req, 0, sizeof(*req));

    /* Parse "GET /path?query HTTP/1.x" */
    char url[768];
    if (sscanf(raw, "%7s %767s", req->method, url) != 2)
        return -1;

    /* Split path and query string */
    char *q = strchr(url, '?');
    if (q) {
        *q = '\0';
        strncpy(req->query, q + 1, sizeof(req->query) - 1);
    }
    strncpy(req->path, url, sizeof(req->path) - 1);
    return 0;
}

const char *http_query_param(const char *query, const char *key, char *value, int value_size) {
    char search[128];
    snprintf(search, sizeof(search), "%s=", key);

    const char *start = strstr(query, search);
    if (!start) return NULL;

    start += strlen(search);
    const char *end = strchr(start, '&');
    int len = end ? (int)(end - start) : (int)strlen(start);
    if (len >= value_size) len = value_size - 1;

    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

void http_send_response(int client_fd, int status, const char *content_type, const char *body) {
    char header[512];
    int body_len = body ? (int)strlen(body) : 0;
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status, content_type, body_len);
    write(client_fd, header, hlen);
    if (body && body_len > 0)
        write(client_fd, body, body_len);
}

void http_send_file(int client_fd, const char *filepath, const char *content_type) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        http_send_response(client_fd, 404, "text/plain", "Not Found");
        return;
    }

    struct stat st;
    fstat(fd, &st);
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        content_type, (long)st.st_size);
    write(client_fd, header, hlen);

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(client_fd, buf, n);

    close(fd);
}
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/http.c src/http.h
git commit -m "módulo HTTP: parsing de requisições e envio de respostas"
```

---

### Task 6: JSON Module

**Files:**
- Create: `src/json.h`
- Create: `src/json.c`

Generates JSON strings for each endpoint, using snprintf. All JSON formats match the PDF spec exactly.

- [ ] **Step 1: Define json.h**

```c
#ifndef JSON_H
#define JSON_H

#include "game.h"

/* Generate JSON for GET /tiro response. Returns length written. */
int json_shot_result(char *buf, int buf_size, const Shot *shot);

/* Generate JSON for GET /status response. Returns length written. */
int json_status(char *buf, int buf_size, GameState *state);

/* Generate JSON for GET /estado_local response. Returns length written. */
int json_local_state(char *buf, int buf_size, GameState *state);

#endif
```

- [ ] **Step 2: Implement json.c**

```c
#include "json.h"
#include <stdio.h>
#include <string.h>

int json_shot_result(char *buf, int buf_size, const Shot *shot) {
    if (strcmp(shot->result, "acerto") == 0) {
        return snprintf(buf, buf_size,
            "{\"resultado\":\"acerto\",\"tipo\":\"%s\",\"pontos\":%d}",
            shot->ship_type, /* points lookup needed */
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

    /* Ships array */
    offset += snprintf(buf + offset, buf_size - offset, "{\"navios\":[");
    for (int i = 0; i < NUM_SHIPS; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        Ship *s = &state->ships[i];
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"id\":%d,\"tipo\":\"%s\",\"linha\":%d,\"coluna\":%d,\"vivo\":%s}",
            s->id, s->type, s->row, s->col,
            s->alive ? "true" : "false");
    }

    /* Shots array */
    offset += snprintf(buf + offset, buf_size - offset, "],\"tiros\":[");
    for (int i = 0; i < state->shot_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        Shot *t = &state->shots[i];
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"linha\":%d,\"coluna\":%d,\"resultado\":\"%s\",\"atacante\":\"%s\"}",
            t->row, t->col, t->result, t->attacker_ip);
    }

    offset += snprintf(buf + offset, buf_size - offset,
        "],\"pontuacao_sofrida\":%d}", state->score_suffered);

    return offset;
}
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/json.c src/json.h
git commit -m "módulo JSON: geração de respostas /tiro, /status e /estado_local"
```

---

### Task 7: Server Module (select loop + request dispatch)

**Files:**
- Create: `src/server.h`
- Create: `src/server.c`

- [ ] **Step 1: Define server.h**

```c
#ifndef SERVER_H
#define SERVER_H

#include "game.h"

#define SERVER_PORT 8080
#define MAX_REQUEST_SIZE 2048

/* Create and bind the listening TCP socket. Returns fd or -1. */
int server_create_socket(void);

/* Main select() loop. Monitors http_fd and fifo_fd. Never returns. */
void server_run(int http_fd, int fifo_fd, GameState *state);

#endif
```

- [ ] **Step 2: Implement server.c**

```c
#include "server.h"
#include "http.h"
#include "json.h"
#include "fifo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int server_create_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return -1;
    if (listen(fd, 10) < 0) return -1;

    return fd;
}

static void handle_fifo_data(int fifo_fd, GameState *state) {
    char buf[FIFO_BUF_SIZE];
    ssize_t n = read(fifo_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    /* Process each line in buffer (may contain multiple messages) */
    char *line = strtok(buf, "\n");
    while (line) {
        int ship_id, col;
        if (fifo_parse_message(line, &ship_id, &col) == 0)
            game_update_ship_col(state, ship_id, col);
        line = strtok(NULL, "\n");
    }
}

static void handle_http_request(int client_fd, const char *client_ip, GameState *state) {
    char raw[MAX_REQUEST_SIZE];
    ssize_t n = read(client_fd, raw, sizeof(raw) - 1);
    if (n <= 0) { close(client_fd); return; }
    raw[n] = '\0';

    HttpRequest req;
    if (http_parse_request(raw, &req) < 0) {
        http_send_response(client_fd, 400, "text/plain", "Bad Request");
        close(client_fd);
        return;
    }
    strncpy(req.client_ip, client_ip, sizeof(req.client_ip) - 1);

    char json_buf[8192];

    if (strcmp(req.path, "/tiro") == 0) {
        char row_str[16], col_str[16];
        if (!http_query_param(req.query, "linha", row_str, sizeof(row_str)) ||
            !http_query_param(req.query, "coluna", col_str, sizeof(col_str))) {
            http_send_response(client_fd, 400, "application/json",
                "{\"erro\":\"parametros linha e coluna obrigatorios\"}");
            close(client_fd);
            return;
        }
        int row = atoi(row_str);
        int col = atoi(col_str);

        Shot *shot = game_process_shot(state, row, col, req.client_ip);

        /* If hit, kill the ship process */
        if (strcmp(shot->result, "acerto") == 0) {
            for (int i = 0; i < NUM_SHIPS; i++) {
                if (state->ships[i].row == row && !state->ships[i].alive && state->ships[i].pid > 0) {
                    kill(state->ships[i].pid, SIGTERM);
                    state->ships[i].pid = 0;
                    break;
                }
            }
        }

        json_shot_result(json_buf, sizeof(json_buf), shot);
        http_send_response(client_fd, 200, "application/json", json_buf);

    } else if (strcmp(req.path, "/status") == 0) {
        json_status(json_buf, sizeof(json_buf), state);
        http_send_response(client_fd, 200, "application/json", json_buf);

    } else if (strcmp(req.path, "/estado_local") == 0) {
        json_local_state(json_buf, sizeof(json_buf), state);
        http_send_response(client_fd, 200, "application/json", json_buf);

    } else if (strcmp(req.path, "/") == 0 || strcmp(req.path, "/index.html") == 0) {
        http_send_file(client_fd, "index.html", "text/html");

    } else {
        http_send_response(client_fd, 404, "text/plain", "Not Found");
    }

    close(client_fd);
}

void server_run(int http_fd, int fifo_fd, GameState *state) {
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(http_fd, &read_fds);
        FD_SET(fifo_fd, &read_fds);

        int max_fd = (http_fd > fifo_fd) ? http_fd : fifo_fd;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(fifo_fd, &read_fds))
            handle_fifo_data(fifo_fd, state);

        if (FD_ISSET(http_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(http_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (client_fd < 0) continue;

            char client_ip[46];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            handle_http_request(client_fd, client_ip, state);
        }
    }
}
```

- [ ] **Step 3: Verify compilation**

Run: `make`
Expected: Compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add src/server.c src/server.h
git commit -m "módulo servidor: socket TCP, select() loop e dispatch de requisições"
```

---

### Task 8: Main Entry Point (wire everything together)

**Files:**
- Create: `src/main.c`

- [ ] **Step 1: Implement main.c**

```c
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
            /* Child: run ship logic */
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
```

- [ ] **Step 2: Build and test manually**

Run: `make clean && make`
Expected: Compiles without errors.

Run: `./miniwebserver &` then:
- `curl http://127.0.0.1:8080/status` → JSON with ship lines and counts
- `curl http://127.0.0.1:8080/estado_local` → JSON with full state
- `curl "http://127.0.0.1:8080/tiro?linha=0&coluna=0"` → agua, acerto, or repetido

Kill with: `kill %1`

- [ ] **Step 3: Commit**

```bash
git add src/main.c
git commit -m "entry point: inicialização do jogo, fork dos navios e start do servidor"
```

---

## Chunk 3: HTML Page, Attacker, Polish

### Task 9: HTML Visualization Page

**Files:**
- Create: `index.html`

- [ ] **Step 1: Create index.html**

A single HTML file with inline CSS and minimal JS. Polls `/estado_local` every 2 seconds, renders a 10x10 grid showing ships and shots.

The page should contain:
- A `<table>` rendered as a 10x10 grid
- Ship cells colored by type (e.g., green for fragata, blue for submarino, red for porta-aviões)
- Shot cells marked (X for hit, O for miss)
- Ship list showing alive/dead status
- Shot log table
- Score display
- A `setInterval(fetchState, 2000)` polling loop

- [ ] **Step 2: Test in browser**

Run: `./miniwebserver &`
Open: `http://127.0.0.1:8080/` in browser.
Expected: Board renders, ships appear, updates every 2s.

- [ ] **Step 3: Commit**

```bash
git add index.html
git commit -m "página HTML: visualização do tabuleiro com polling automático"
```

---

### Task 10: Attacker Process

**Files:**
- Create: `src/attacker.c`
- Create: `alvos.txt`

- [ ] **Step 1: Create alvos.txt with placeholder**

```
# One IP per line
127.0.0.1
```

- [ ] **Step 2: Implement attacker.c**

A standalone program that:
1. Reads IPs from `alvos.txt`
2. For each IP: sends `GET /status`, parses JSON to find ship rows
3. Sorts rows by ship value (porta_avioes first)
4. Sweeps all 10 columns for each row via `GET /tiro?linha=L&coluna=C`
5. Tracks results (hits, misses, ships sunk)
6. Respects the 100 shot limit
7. Prints a final report

Uses raw TCP sockets to make HTTP requests (no libcurl).

Key functions:
- `read_targets(filename)` — reads IPs from file
- `http_get(ip, port, path, response_buf)` — sends GET and reads response
- `parse_status_json(response)` — extracts ship rows/types
- `attack_target(ip)` — executes the sweep strategy
- `print_report()` — prints summary

- [ ] **Step 3: Build and test against local server**

Run: `make`
Run: `./miniwebserver &` then `./atacante`
Expected: Attacker queries local server, fires shots, prints report with hits/misses.

- [ ] **Step 4: Commit**

```bash
git add src/attacker.c alvos.txt
git commit -m "processo atacante: consulta status, varre colunas e gera relatório"
```

---

### Task 11: Integration Test & Cleanup

- [ ] **Step 1: Full integration test**

```bash
make clean && make
./miniwebserver &
sleep 2

# Test all endpoints
curl -s http://127.0.0.1:8080/status | python3 -m json.tool
curl -s http://127.0.0.1:8080/estado_local | python3 -m json.tool
curl -s "http://127.0.0.1:8080/tiro?linha=0&coluna=0"
curl -s "http://127.0.0.1:8080/tiro?linha=0&coluna=0"  # Should return "repetido"

# Test attacker
./atacante

# Test HTML page
echo "Open http://127.0.0.1:8080/ in browser and verify board renders"

kill %1
```

- [ ] **Step 2: Fix any issues found**

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "testes de integração e ajustes finais"
```
