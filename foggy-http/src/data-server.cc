#include "server_common.h"

static int date_handler(const foggy_http_request_t *request,
                        foggy_http_response_t *response, void *context) {
    (void)request;
    (void)context;
    time_t now = time(NULL);
    struct tm tm;
    char date[32];
    if (!localtime_r(&now, &tm)) return -1;
    size_t size = strftime(date, sizeof(date), "%Y-%m-%d\n", &tm);
    if (foggy_http_header_set(&response->headers, "Content-Type", "text/plain; charset=utf-8") < 0) return -1;
    return foggy_http_buffer_append(&response->body, date, size);
}

int main(int argc, char **argv) {
    int port;
    if (server_options(argc, argv, 8081, &port, NULL) < 0) {
        fprintf(stderr, "usage: data-server [--port PORT]\n");
        return 1;
    }
    if (setenv("TZ", "Asia/Hong_Kong", 1) < 0) {
        perror("ERROR timezone");
        return 1;
    }
    tzset();
    foggy_http_server_t server;
    foggy_http_server_init(&server);
    int result = foggy_http_server_get(&server, "/checkpoint-3/date", date_handler, NULL);
    if (result == 0) {
        printf("READY date %d\n", port);
        fflush(stdout);
        result = foggy_http_server_listen(&server, port, 0);
    }
    if (result < 0) perror("ERROR server");
    foggy_http_server_free(&server);
    return result < 0 ? 1 : 0;
}
