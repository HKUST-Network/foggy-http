#ifndef APP_SERVER_COMMON_H
#define APP_SERVER_COMMON_H
#include "foggy_http.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int server_options(int argc, char **argv, int default_port, int *port, int *chunked) {
    *port = default_port;
    if (chunked) *chunked = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--chunked") && chunked) *chunked = 1;
        else if (!strcmp(argv[i], "--port")) {
            if (++i == argc) return -1;
            char *end;
            errno = 0;
            long value = strtol(argv[i], &end, 10);
            if (errno || end == argv[i] || *end || value < 1 || value > 65535) return -1;
            *port = (int)value;
        } else return -1;
    }
    return 0;
}
#endif
