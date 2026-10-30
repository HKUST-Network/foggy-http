#include "foggy_http.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int foggy_http_buffer_append(foggy_http_buffer_t *buffer, const void *data, size_t size) {
    if (size == 0) return 0;
    if (!data || size > SIZE_MAX - buffer->size - 1) { errno = EINVAL; return -1; }
    size_t needed = buffer->size + size + 1;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 256;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) { capacity = needed; break; }
            capacity *= 2;
        }
        char *grown = realloc(buffer->data, capacity);
        if (!grown) return -1;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';
    return 0;
}

void foggy_http_buffer_free(foggy_http_buffer_t *buffer) {
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

int foggy_http_header_set(foggy_http_header_t **headers, const char *name, const char *value) {
    char *copy = strdup(value);
    if (!copy) return -1;
    for (foggy_http_header_t *item = *headers; item; item = item->next) {
        if (strcasecmp(item->name, name) == 0) {
            free(item->value);
            item->value = copy;
            return 0;
        }
    }
    foggy_http_header_t *item = calloc(1, sizeof(*item));
    if (!item) { free(copy); return -1; }
    item->name = strdup(name);
    if (!item->name) { free(copy); free(item); return -1; }
    item->value = copy;
    item->next = *headers;
    *headers = item;
    return 0;
}

const char *foggy_http_header_get(const foggy_http_header_t *headers, const char *name) {
    for (; headers; headers = headers->next)
        if (strcasecmp(headers->name, name) == 0) return headers->value;
    return NULL;
}

void foggy_http_headers_free(foggy_http_header_t *headers) {
    while (headers) {
        foggy_http_header_t *next = headers->next;
        free(headers->name);
        free(headers->value);
        free(headers);
        headers = next;
    }
}

void foggy_http_response_init(foggy_http_response_t *response) {
    memset(response, 0, sizeof(*response));
    response->status = 200;
}

void foggy_http_response_free(foggy_http_response_t *response) {
    foggy_http_buffer_free(&response->body);
    foggy_http_headers_free(response->headers);
    if (response->free_context) response->free_context(response->next_context);
    memset(response, 0, sizeof(*response));
}

int foggy_http_client_init(foggy_http_client_t *client, const char *host, int port, int keep_alive) {
    memset(client, 0, sizeof(*client));
    if (!host || port < 1 || port > 65535) { errno = EINVAL; return -1; }
    client->host = strdup(host);
    if (!client->host) return -1;
    client->port = port;
    client->keep_alive = keep_alive;
    return 0;
}

void foggy_http_client_close(foggy_http_client_t *client) {
    foggy_transport_close(&client->connection);
    foggy_http_buffer_free(&client->input);
}

void foggy_http_client_free(foggy_http_client_t *client) {
    foggy_http_client_close(client);
    free(client->host);
    memset(client, 0, sizeof(*client));
}

int foggy_http_client_connection_port(const foggy_http_client_t *client) {
    return foggy_transport_local_port(client->connection);
}

/* Deliberately incomplete starter: raw application bytes, NOT valid HTTP/1.0.
 * These TODOs and the one-exchange-per-connection behavior match the C++ starter. */
static int request(foggy_http_client_t *client, const char *method, const char *path,
                   const void *body, size_t body_size, foggy_http_receiver_t receiver,
                   void *context, foggy_http_response_t *response) {
    /* TODO: serialize start-line, headers, Content-Length, Host and Connection. */
    /* TODO: reuse a live connection; parse responses and decode chunked incrementally. */
    foggy_http_client_close(client);
    client->connection = foggy_transport_open(client->host, client->port, 0);
    if (!client->connection) return -1;
    const void *data = strcmp(method, "GET") == 0 ? path : body;
    size_t size = strcmp(method, "GET") == 0 ? strlen(path) : body_size;
    if (foggy_transport_write(client->connection, data, size) < 0) goto fail;
    for (;;) {
        char data_in[4096];
        int n = foggy_transport_read(client->connection, data_in, sizeof(data_in), 5000);
        if (n < 0) goto fail;
        if (n == 0) break;
        if (foggy_http_buffer_append(&response->body, data_in, (size_t)n) < 0) goto fail;
    }
    /* One buffered delivery, not streaming. */
    if (receiver) receiver(response->body.data, response->body.size, context);
    foggy_http_client_close(client);
    return 0;
fail: {
    int saved = errno;
    foggy_http_client_close(client);
    errno = saved;
    return -1;
}
}

int foggy_http_client_get(foggy_http_client_t *client, const char *path, foggy_http_response_t *response) {
    return request(client, "GET", path, NULL, 0, NULL, NULL, response);
}

int foggy_http_client_post(foggy_http_client_t *client, const char *path,
                           const void *body, size_t body_size,
                           foggy_http_receiver_t receiver, void *context,
                           foggy_http_response_t *response) {
    return request(client, "POST", path, body, body_size, receiver, context, response);
}

void foggy_http_server_init(foggy_http_server_t *server) { server->routes = NULL; }

static int route(foggy_http_server_t *server, const char *method, const char *path,
                 foggy_http_handler_t handler, void *context) {
    for (foggy_http_route_t *item = server->routes; item; item = item->next) {
        if (strcmp(item->method, method) == 0 && strcmp(item->path, path) == 0) {
            item->handler = handler;
            item->context = context;
            return 0;
        }
    }
    foggy_http_route_t *item = calloc(1, sizeof(*item));
    if (!item) return -1;
    item->path = strdup(path);
    if (!item->path) { free(item); return -1; }
    item->method = method;
    item->handler = handler;
    item->context = context;
    item->next = server->routes;
    server->routes = item;
    return 0;
}

int foggy_http_server_get(foggy_http_server_t *server, const char *path,
                          foggy_http_handler_t handler, void *context) {
    return route(server, "GET", path, handler, context);
}

int foggy_http_server_post(foggy_http_server_t *server, const char *path,
                           foggy_http_handler_t handler, void *context) {
    return route(server, "POST", path, handler, context);
}

void foggy_http_server_free(foggy_http_server_t *server) {
    while (server->routes) {
        foggy_http_route_t *next = server->routes->next;
        free(server->routes->path);
        free(server->routes);
        server->routes = next;
    }
}

int foggy_http_server_listen(foggy_http_server_t *server, int port, int session_limit) {
    for (int session = 0; session_limit == 0 || session < session_limit; ++session) {
        foggy_transport_t io = foggy_transport_open("0.0.0.0", port, 1);
        if (!io) return -1;
        char data[4096];
        int n = foggy_transport_read(io, data, sizeof(data), 30000);
        if (n <= 0 || !server->routes) { foggy_transport_close(&io); continue; }
        /* TODO: maintain a per-connection buffer and parse full HTTP requests.
         * Raw starter handles one small payload in one read only. */
        foggy_http_request_t request = {0};
        request.version = "HTTP/1.1";
        request.body = data;
        request.body_size = (size_t)n;
        foggy_http_response_t response;
        foggy_http_response_init(&response);
        foggy_http_route_t *handler = server->routes;
        int result = handler->handler(&request, &response, handler->context);
        if (result == 0 && response.next) {
            int more;
            do {
                foggy_http_buffer_t part = {0};
                more = response.next(response.next_context, &part);
                result = more < 0 ? -1 : foggy_http_buffer_append(&response.body, part.data, part.size);
                foggy_http_buffer_free(&part);
            } while (result == 0 && more > 0);
        }
        /* TODO: serialize response; chunk producer output; loop for keep-alive. */
        if (result == 0) result = foggy_transport_write(io, response.body.data, response.body.size);
        int saved = errno;
        foggy_http_response_free(&response);
        foggy_transport_close(&io);
        if (result < 0) { errno = saved; return -1; }
    }
    return 0;
}
