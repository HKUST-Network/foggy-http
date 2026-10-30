#include "client_common.h"
#include <inttypes.h>

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
    client_options_t o;
    if (client_options(argc, argv, 1, &o) < 0) {
        fprintf(stderr, "usage: echo-client HOST [TEXT] [--port PORT] [--no-keep-alive] [--repeat N] [--interval S]\n");
        return 1;
    }
    foggy_http_client_t client;
    if (foggy_http_client_init(&client, o.host, o.port, o.keep_alive) < 0) { perror("ERROR client"); return 1; }
    for (int i = 0; i < o.repeat; ++i) {
        if (i) client_interval(o.interval);
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        foggy_http_response_t response;
        foggy_http_response_init(&response);
        int result = foggy_http_client_post(&client, "/checkpoint-3/echo", o.text, strlen(o.text),
                                           receive, &start, &response);
        if (result < 0) {
            perror("ERROR request");
            foggy_http_response_free(&response);
            foggy_http_client_free(&client);
            return 1;
        }
        fprintf(stderr, "RESPONSE %d local_port=%d\n", response.status,
                foggy_http_client_connection_port(&client));
        foggy_http_response_free(&response);
    }
    foggy_http_client_free(&client);
    return 0;
}
