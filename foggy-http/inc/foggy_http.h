#ifndef FOGGY_HTTP_H
#define FOGGY_HTTP_H

#include "transport.h"
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *data;
    size_t size, capacity;
} foggy_http_buffer_t;

typedef struct foggy_http_header {
    char *name, *value;
    struct foggy_http_header *next;
} foggy_http_header_t;

/* Start-line strings and body are borrowed until the handler returns. */
typedef struct {
    const char *method, *target, *version;
    foggy_http_header_t *headers;
    const void *body;
    size_t body_size;
} foggy_http_request_t;

typedef void (*foggy_http_receiver_t)(const void *data, size_t size, void *context);
/* 1 = more output; 0 = last output (possibly nonempty); -1 = error. */
typedef int (*foggy_http_next_t)(void *context, foggy_http_buffer_t *output);

typedef struct {
    int status;
    foggy_http_buffer_t body;
    foggy_http_header_t *headers;
    foggy_http_next_t next;
    void *next_context;
    void (*free_context)(void *context);
} foggy_http_response_t;

typedef int (*foggy_http_handler_t)(const foggy_http_request_t *request,
                                  foggy_http_response_t *response, void *context);

typedef struct {
    char *host;
    int port, keep_alive;
    foggy_transport_t connection;
    foggy_http_buffer_t input;
    struct timespec last_used;
} foggy_http_client_t;

typedef struct foggy_http_route {
    const char *method;
    char *path;
    foggy_http_handler_t handler;
    void *context;
    struct foggy_http_route *next;
} foggy_http_route_t;

typedef struct {
    foggy_http_route_t *routes;
} foggy_http_server_t;

/* Zero-initialize buffers/header lists. Helpers return 0 or -1 with errno.
 * Appended data must not alias the destination buffer. Bodies are binary-safe. */
int foggy_http_buffer_append(foggy_http_buffer_t *buffer, const void *data, size_t size);
void foggy_http_buffer_free(foggy_http_buffer_t *buffer);
int foggy_http_header_set(foggy_http_header_t **headers, const char *name, const char *value);
const char *foggy_http_header_get(const foggy_http_header_t *headers, const char *name);
void foggy_http_headers_free(foggy_http_header_t *headers);

/* Initialize each response before use; free it after success or failure.
 * free_context, when provided, releases producer state exactly once. */
void foggy_http_response_init(foggy_http_response_t *response);
void foggy_http_response_free(foggy_http_response_t *response);

int foggy_http_client_init(foggy_http_client_t *client, const char *host, int port, int keep_alive);
int foggy_http_client_get(foggy_http_client_t *client, const char *path, foggy_http_response_t *response);
int foggy_http_client_post(foggy_http_client_t *client, const char *path,
                           const void *body, size_t body_size,
                           foggy_http_receiver_t receiver, void *context,
                           foggy_http_response_t *response);
void foggy_http_client_close(foggy_http_client_t *client);
void foggy_http_client_free(foggy_http_client_t *client);
int foggy_http_client_connection_port(const foggy_http_client_t *client);

/* Route contexts are borrowed; server_free does not release them. */
void foggy_http_server_init(foggy_http_server_t *server);
int foggy_http_server_get(foggy_http_server_t *server, const char *path,
                          foggy_http_handler_t handler, void *context);
int foggy_http_server_post(foggy_http_server_t *server, const char *path,
                           foggy_http_handler_t handler, void *context);
/* Sequential connections only. session_limit=0 runs until termination. */
int foggy_http_server_listen(foggy_http_server_t *server, int port, int session_limit);
void foggy_http_server_free(foggy_http_server_t *server);

#ifdef __cplusplus
}
#endif
#endif
