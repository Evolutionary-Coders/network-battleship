#include "http.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int http_parse_request(const char *raw, HttpRequest *req) {
    memset(req, 0, sizeof(*req));

    char url[768];
    if (sscanf(raw, "%7s %767s", req->method, url) != 2)
        return -1;

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
