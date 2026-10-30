#include "foggy_http.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int number(const char *text, int *value) {
    char *end;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno || end == text || *end || parsed < 0 || parsed > INT_MAX) return -1;
    *value = (int)parsed;
    return 0;
}

static void receive(const void *data, size_t size, void *context) {
    const struct timespec *start = context;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t ms = (int64_t)(now.tv_sec - start->tv_sec) * 1000
               + (now.tv_nsec - start->tv_nsec) / 1000000;
    if (size) fwrite(data, 1, size, stdout);
    fflush(stdout);
    fprintf(stderr, "CHUNK %" PRId64 " %zu\n", ms, size);
}

int main(int argc, char **argv) {
    if (argc < 4) goto usage;
    const char *task = argv[3], *text = "Foggy HTTP streaming";
    int chunks = 0, keep = 1, repeat = 1, interval = 0, port;
    if (number(argv[2], &port) < 0 || port < 1 || port > 65535) goto usage;
    if (strcmp(task, "date") && strcmp(task, "echo")) goto usage;
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--chunked") == 0) chunks = 1;
        else if (strcmp(argv[i], "--no-keep-alive") == 0) keep = 0;
        else if (strcmp(argv[i], "--repeat") == 0) {
            if (++i == argc || number(argv[i], &repeat) < 0 || repeat == 0) goto usage;
        } else if (strcmp(argv[i], "--interval") == 0) {
            if (++i == argc || number(argv[i], &interval) < 0) goto usage;
        } else text = argv[i];
    }
    foggy_http_client_t client;
    if (foggy_http_client_init(&client, argv[1], port, keep) < 0) { perror("ERROR client"); return 1; }
    for (int i = 0; i < repeat; ++i) {
        if (i) {
            struct timespec delay = {interval, 0};
            while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
        }
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        foggy_http_response_t response;
        foggy_http_response_init(&response);
        int result = strcmp(task, "date") == 0
            ? foggy_http_client_get(&client, "/date", &response)
            : foggy_http_client_post(&client, "/echo", text, strlen(text),
                                      chunks ? receive : NULL, &start, &response);
        if (result < 0) {
            perror("ERROR request");
            foggy_http_response_free(&response);
            foggy_http_client_free(&client);
            return 1;
        }
        if ((!chunks || strcmp(task, "date") == 0) && response.body.size)
            fwrite(response.body.data, 1, response.body.size, stdout);
        fflush(stdout);
        fprintf(stderr, "RESPONSE %d local_port=%d\n", response.status,
                foggy_http_client_connection_port(&client));
        foggy_http_response_free(&response);
    }
    foggy_http_client_free(&client);
    return 0;
usage:
    fprintf(stderr, "usage: foggy-http-client HOST PORT date|echo [TEXT] [--chunked] [--no-keep-alive] [--repeat N] [--interval S]\n");
    return 1;
}
