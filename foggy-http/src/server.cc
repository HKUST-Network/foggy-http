#include "foggy_http.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    foggy_http_buffer_t text;
    size_t offset;
} echo_context_t;

static void free_echo(void *context) {
    echo_context_t *echo = context;
    foggy_http_buffer_free(&echo->text);
    free(echo);
}

static int next_echo(void *context, foggy_http_buffer_t *out) {
    echo_context_t *echo = context;
    if (echo->offset == echo->text.size) return 0;
    /* ASCII demo: one character per 100 ms. */
    struct timespec delay = {0, 100000000};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
    if (foggy_http_buffer_append(out, echo->text.data + echo->offset, 1) < 0) return -1;
    ++echo->offset;
    return echo->offset < echo->text.size;
}

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
    if (argc < 3 || argc > 4) goto usage;
    int chunked = argc == 4 && strcmp(argv[3], "--chunked") == 0;
    if (argc == 4 && !chunked) goto usage;
    char *end;
    errno = 0;
    long port = strtol(argv[2], &end, 10);
    if (errno || end == argv[2] || *end || port < 1 || port > 65535) goto usage;
    foggy_http_server_t server;
    foggy_http_server_init(&server);
    int result;
    if (strcmp(argv[1], "date") == 0)
        result = foggy_http_server_get(&server, "/date", date_handler, NULL);
    else if (strcmp(argv[1], "echo") == 0)
        result = foggy_http_server_post(&server, "/echo", echo_handler, &chunked);
    else goto usage;
    if (result == 0) {
        printf("READY %s %s\n", argv[1], argv[2]);
        fflush(stdout);
        result = foggy_http_server_listen(&server, (int)port, 0);
    }
    if (result < 0) perror("ERROR server");
    foggy_http_server_free(&server);
    return result < 0 ? 1 : 0;
usage:
    fprintf(stderr, "usage: foggy-http-server date|echo PORT [--chunked]\n");
    return 1;
}
