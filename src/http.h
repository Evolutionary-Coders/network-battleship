#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char method[8];
    char path[256];
    char query[512];
    char client_ip[46];
} HttpRequest;

int http_parse_request(const char *raw, HttpRequest *req);
const char *http_query_param(const char *query, const char *key, char *value, int value_size);
void http_send_response(int client_fd, int status, const char *content_type, const char *body);
void http_send_file(int client_fd, const char *filepath, const char *content_type);

#endif
