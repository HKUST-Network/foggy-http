#include "client_common.h"

int main(int argc, char **argv) {
    client_options_t o;
    if (client_options(argc, argv, 0, &o) < 0) {
        fprintf(stderr, "usage: date-client HOST [--port PORT] [--no-keep-alive] [--repeat N] [--interval S]\n");
        return 1;
    }
    foggy_http_client_t client;
    if (foggy_http_client_init(&client, o.host, o.port, o.keep_alive) < 0) { perror("ERROR client"); return 1; }
    for (int i = 0; i < o.repeat; ++i) {
        if (i) client_interval(o.interval);
        foggy_http_response_t response;
        foggy_http_response_init(&response);
        int result = foggy_http_client_get(&client, "/checkpoint-3/date", &response);
        if (result < 0) {
            perror("ERROR request");
            foggy_http_response_free(&response);
            foggy_http_client_free(&client);
            return 1;
        }
        if (response.body.size) fwrite(response.body.data, 1, response.body.size, stdout);
        fflush(stdout);
        fprintf(stderr, "RESPONSE %d local_port=%d\n", response.status,
                foggy_http_client_connection_port(&client));
        foggy_http_response_free(&response);
    }
    foggy_http_client_free(&client);
    return 0;
}
