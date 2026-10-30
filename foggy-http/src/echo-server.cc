#include "server_common.h"

typedef struct { foggy_http_buffer_t text; size_t offset; } echo_context_t;

static void free_echo(void *context) {
    echo_context_t *echo = context;
    foggy_http_buffer_free(&echo->text);
    free(echo);
}

static int next_echo(void *context, foggy_http_buffer_t *out) {
    echo_context_t *echo = context;
    if (echo->offset == echo->text.size) return 0;
    struct timespec delay = {0, 100000000};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
    if (foggy_http_buffer_append(out, echo->text.data + echo->offset, 1) < 0) return -1;
    ++echo->offset;
    return echo->offset < echo->text.size;
}

static int echo_handler(const foggy_http_request_t *request,
                        foggy_http_response_t *response, void *context) {
    int chunked = *(const int *)context;
    if (foggy_http_header_set(&response->headers, "Content-Type", "text/plain; charset=utf-8") < 0) return -1;
    if (!chunked) return foggy_http_buffer_append(&response->body, request->body, request->body_size);
    echo_context_t *echo = calloc(1, sizeof(*echo));
    if (!echo) return -1;
    if (foggy_http_buffer_append(&echo->text, request->body, request->body_size) < 0) {
        free_echo(echo);
        return -1;
    }
    response->next = next_echo;
    response->next_context = echo;
    response->free_context = free_echo;
    return 0;
}

int main(int argc, char **argv) {
    int port, chunked;
    if (server_options(argc, argv, 8082, &port, &chunked) < 0) {
        fprintf(stderr, "usage: echo-server [--port PORT] [--chunked]\n");
        return 1;
    }
    foggy_http_server_t server;
    foggy_http_server_init(&server);
    int result = foggy_http_server_post(&server, "/checkpoint-3/echo", echo_handler, &chunked);
    if (result == 0) {
        printf("READY echo %d\n", port);
        fflush(stdout);
        result = foggy_http_server_listen(&server, port, 0);
    }
    if (result < 0) perror("ERROR server");
    foggy_http_server_free(&server);
    return result < 0 ? 1 : 0;
}
