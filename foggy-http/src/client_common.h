#ifndef APP_CLIENT_COMMON_H
#define APP_CLIENT_COMMON_H
#include "foggy_http.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *host, *text;
    int port, keep_alive, repeat, interval;
} client_options_t;

static int app_number(const char *s, int *out) {
    char *end;
    errno = 0;
    long n = strtol(s, &end, 10);
    if (errno || end == s || *end || n < 0 || n > INT_MAX) return -1;
    *out = (int)n;
    return 0;
}

static int client_options(int argc, char **argv, int echo, client_options_t *o) {
    if (argc < 2) return -1;
    *o = (client_options_t){argv[1], "Foggy HTTP streaming", 80, 1, 1, 0};
    int text_seen = 0;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-keep-alive")) o->keep_alive = 0;
        else if (!strcmp(argv[i], "--port")) {
            if (++i == argc || app_number(argv[i], &o->port) < 0 || o->port < 1 || o->port > 65535) return -1;
        } else if (!strcmp(argv[i], "--repeat")) {
            if (++i == argc || app_number(argv[i], &o->repeat) < 0 || o->repeat < 1) return -1;
        } else if (!strcmp(argv[i], "--interval")) {
            if (++i == argc || app_number(argv[i], &o->interval) < 0) return -1;
        } else if (echo && !text_seen && strncmp(argv[i], "--", 2)) {
            o->text = argv[i];
            text_seen = 1;
        } else return -1;
    }
    return 0;
}

static void client_interval(int seconds) {
    struct timespec delay = {seconds, 0};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
}
#endif
