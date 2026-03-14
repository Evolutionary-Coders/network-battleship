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

    } else if (strcmp(req.path, "/style.css") == 0) {
        http_send_file(client_fd, "style.css", "text/css");

    } else if (strcmp(req.path, "/app.js") == 0) {
        http_send_file(client_fd, "app.js", "application/javascript");

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
