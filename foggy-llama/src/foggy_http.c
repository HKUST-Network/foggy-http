#include "foggy_http.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
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


#define HTTP_LINE_LIMIT 8192U
#define HTTP_HEAD_LIMIT 16384U
#define HTTP_BODY_LIMIT (1024U * 1024U)
#ifndef HTTP_READ_MS
#define HTTP_READ_MS 120000
#endif
#define HTTP_IDLE_MS 3000

typedef struct {
    char *storage;
    const char *method, *target, *version;
    foggy_http_header_t *headers;
    size_t length;
    int has_length, chunked, persistent, status;
} message_t;

static int protocol_error(void) { errno = EPROTO; return -1; }

static int64_t milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void consume(foggy_http_buffer_t *input, size_t count) {
    input->size -= count;
    if (input->size) memmove(input->data, input->data + count, input->size);
    if (input->data) input->data[input->size] = '\0';
}

/* A deadline spans the whole incomplete message, rather than each read. */
static int read_more(foggy_transport_t io, foggy_http_buffer_t *input, int64_t deadline) {
    int64_t remaining = deadline - milliseconds();
    if (remaining <= 0) { errno = ETIMEDOUT; return -1; }
    char data[4096];
    int n = foggy_transport_read(io, data, sizeof(data), (int)remaining);
    if (n < 0) {
        if (n == -2) errno = ETIMEDOUT;
        return -1;
    }
    if (n && foggy_http_buffer_append(input, data, (size_t)n) < 0) return -1;
    return n;
}

static int token_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", c) != NULL;
}

static int token(const char *s) {
    if (!*s) return 0;
    for (; *s; ++s) if (!token_char((unsigned char)*s)) return 0;
    return 1;
}

static int header_value(const char *s) {
    for (; *s; ++s)
        if (((unsigned char)*s < 32 && *s != '\t') || (unsigned char)*s == 127) return 0;
    return 1;
}

static int contains_token(const char *value, const char *wanted) {
    if (!value) return 0;
    while (*value) {
        while (*value == ',' || *value == ' ' || *value == '\t') ++value;
        const char *end = strchr(value, ',');
        if (!end) end = value + strlen(value);
        const char *trimmed = end;
        while (trimmed > value && (trimmed[-1] == ' ' || trimmed[-1] == '\t')) --trimmed;
        if ((size_t)(trimmed - value) == strlen(wanted) &&
            strncasecmp(value, wanted, (size_t)(trimmed - value)) == 0) return 1;
        value = *end ? end + 1 : end;
    }
    return 0;
}

/* Stop at the blank line: bytes after it may be arbitrary binary payload. */
static int header_end(const foggy_http_buffer_t *input, size_t *size) {
    size_t start = 0;
    for (size_t i = 0; i < input->size; ++i) {
        unsigned char c = (unsigned char)input->data[i];
        if (i >= HTTP_HEAD_LIMIT) return protocol_error();
        if (c == '\r') {
            if (i - start > HTTP_LINE_LIMIT) return protocol_error();
            if (i + 1 == input->size) return 0;
            if (input->data[i + 1] != '\n') return protocol_error();
            if (i + 2 > HTTP_HEAD_LIMIT) return protocol_error();
            if (i == start) { *size = i + 2; return 1; }
            start = i + 2;
            ++i;
        } else {
            if (c == '\n' || c == 0 || (c < 32 && c != '\t') || c == 127)
                return protocol_error();
            if (i - start >= HTTP_LINE_LIMIT) return protocol_error();
        }
    }
    if (input->size >= HTTP_HEAD_LIMIT) return protocol_error();
    return 0;
}

static void message_free(message_t *message) {
    free(message->storage);
    foggy_http_headers_free(message->headers);
    memset(message, 0, sizeof(*message));
}

static int decimal_length(const char *text, size_t *length) {
    if (!*text) return protocol_error();
    size_t value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return protocol_error();
        unsigned digit = (unsigned)(*text - '0');
        if (value > (HTTP_BODY_LIMIT - digit) / 10) return protocol_error();
        value = value * 10 + digit;
    }
    *length = value;
    return 0;
}

static int parse_header_line(char *line, char **name, char **value) {
    char *colon = strchr(line, ':');
    if (!colon) return protocol_error();
    *colon = '\0';
    if (!token(line)) return protocol_error();
    char *v = colon + 1;
    while (*v == ' ' || *v == '\t') ++v;
    char *end = v + strlen(v);
    while (end > v && (end[-1] == ' ' || end[-1] == '\t')) --end;
    *end = '\0';
    if (!header_value(v)) return protocol_error();
    *name = line;
    *value = v;
    return 0;
}

static int parse_message(message_t *message, int response) {
    char *line = message->storage;
    char *end = strstr(line, "\r\n");
    if (!end) return protocol_error();
    *end = '\0';
    if (response) {
        if (strlen(line) < 12 || line[8] != ' ' || line[12] != ' ' ||
            line[9] < '1' || line[9] > '5' || line[10] < '0' || line[10] > '9' ||
            line[11] < '0' || line[11] > '9') return protocol_error();
        line[8] = '\0';
        message->version = line;
        message->status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
    } else {
        char *space = strchr(line, ' ');
        if (!space) return protocol_error();
        *space = '\0';
        message->method = line;
        message->target = space + 1;
        space = strchr(space + 1, ' ');
        if (!space) return protocol_error();
        *space = '\0';
        message->version = space + 1;
        if (!token(message->method) || !*message->target) return protocol_error();
        for (const char *p = message->target; *p; ++p)
            if ((unsigned char)*p <= 32 || (unsigned char)*p == 127) return protocol_error();
    }
    if (strcmp(message->version, "HTTP/1.0") && strcmp(message->version, "HTTP/1.1"))
        return protocol_error();
    int host_seen = 0, transfer_seen = 0, close_seen = 0, keep_seen = 0;
    line = end + 2;
    while (*line) {
        end = strstr(line, "\r\n");
        if (!end) return protocol_error();
        *end = '\0';
        if (!*line) break;
        char *name, *value;
        if (parse_header_line(line, &name, &value) < 0) return -1;
        if (!strcasecmp(name, "Content-Length")) {
            size_t length;
            if (decimal_length(value, &length) < 0) return -1;
            if (message->has_length && message->length != length) return protocol_error();
            message->has_length = 1;
            message->length = length;
        } else if (!strcasecmp(name, "Transfer-Encoding")) {
            if (transfer_seen++ || strcasecmp(value, "chunked")) return protocol_error();
            message->chunked = 1;
        } else if (!strcasecmp(name, "Host")) {
            if (host_seen++ || !*value) return protocol_error();
        } else if (!strcasecmp(name, "Connection")) {
            close_seen |= contains_token(value, "close");
            keep_seen |= contains_token(value, "keep-alive");
        }
        if (foggy_http_header_set(&message->headers, name, value) < 0) return -1;
        line = end + 2;
    }
    if (message->has_length && message->chunked) return protocol_error();
    if (!response && ((!host_seen && !strcmp(message->version, "HTTP/1.1")) || message->chunked))
        return protocol_error();
    message->persistent = !close_seen && (!strcmp(message->version, "HTTP/1.1") || keep_seen);
    return 0;
}

/* 0 is clean EOF before a message; EOF within a message is a protocol error. */
static int read_header(foggy_transport_t io, foggy_http_buffer_t *input,
                       message_t *message, int response, int64_t deadline) {
    size_t size = 0;
    for (;;) {
        int found = header_end(input, &size);
        if (found < 0) return -1;
        if (found) break;
        int n = read_more(io, input, deadline);
        if (n < 0) return -1;
        if (!n) return input->size ? protocol_error() : 0;
    }
    message->storage = malloc(size + 1);
    if (!message->storage) return -1;
    memcpy(message->storage, input->data, size);
    message->storage[size] = '\0';
    consume(input, size);
    if (parse_message(message, response) < 0) return -1;
    return 1;
}

static int deliver(foggy_http_buffer_t *body, const void *data, size_t size,
                   foggy_http_receiver_t receiver, void *context) {
    if (size > HTTP_BODY_LIMIT - body->size) return protocol_error();
    if (foggy_http_buffer_append(body, data, size) < 0) return -1;
    if (receiver && size) receiver(data, size, context);
    return 0;
}

static int read_payload(foggy_transport_t io, foggy_http_buffer_t *input,
                        foggy_http_buffer_t *body, size_t size,
                        foggy_http_receiver_t receiver, void *context, int64_t deadline) {
    while (size) {
        if (!input->size) {
            int n = read_more(io, input, deadline);
            if (n <= 0) return n < 0 ? -1 : protocol_error();
        }
        size_t count = size < input->size ? size : input->size;
        if (deliver(body, input->data, count, receiver, context) < 0) return -1;
        consume(input, count);
        size -= count;
    }
    return 0;
}

static int read_line(foggy_transport_t io, foggy_http_buffer_t *input,
                     char *line, size_t *length, int64_t deadline) {
    for (;;) {
        for (size_t i = 0; i < input->size; ++i) {
            unsigned char c = (unsigned char)input->data[i];
            if (i > HTTP_LINE_LIMIT) return protocol_error();
            if (c == '\r') {
                if (i + 1 == input->size) break;
                if (input->data[i + 1] != '\n') return protocol_error();
                memcpy(line, input->data, i);
                line[i] = '\0';
                *length = i;
                consume(input, i + 2);
                return 0;
            }
            if (i == HTTP_LINE_LIMIT || c == '\n' || c == 0 || (c < 32 && c != '\t') || c == 127)
                return protocol_error();
        }
        int n = read_more(io, input, deadline);
        if (n <= 0) return n < 0 ? -1 : protocol_error();
    }
}

static int read_chunks(foggy_transport_t io, foggy_http_buffer_t *input,
                       foggy_http_response_t *response, foggy_http_receiver_t receiver,
                       void *context) {
    for (;;) {
        int64_t deadline = milliseconds() + HTTP_READ_MS;
        char line[HTTP_LINE_LIMIT + 1];
        size_t line_size;
        if (read_line(io, input, line, &line_size, deadline) < 0) return -1;
        size_t size = 0, digits = 0;
        for (; line[digits] && line[digits] != ';'; ++digits) {
            unsigned char c = (unsigned char)line[digits];
            unsigned value;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            else return protocol_error();
            if (size > (HTTP_BODY_LIMIT - value) / 16) return protocol_error();
            size = size * 16 + value;
        }
        if (!digits || size > HTTP_BODY_LIMIT - response->body.size) return protocol_error();
        if (!size) {
            size_t trailers = 0;
            do {
                if (read_line(io, input, line, &line_size, deadline) < 0) return -1;
                trailers += line_size + 2;
                if (trailers > HTTP_HEAD_LIMIT) return protocol_error();
                if (line_size) {
                    char *name, *value;
                    if (parse_header_line(line, &name, &value) < 0) return -1;
                    if (!strcasecmp(name, "Content-Length") || !strcasecmp(name, "Transfer-Encoding"))
                        return protocol_error();
                }
            } while (line_size);
            return 0;
        }
        if (read_payload(io, input, &response->body, size, receiver, context, deadline) < 0)
            return -1;
        while (input->size < 2) {
            int n = read_more(io, input, deadline);
            if (n <= 0) return n < 0 ? -1 : protocol_error();
        }
        if (memcmp(input->data, "\r\n", 2)) return protocol_error();
        consume(input, 2);
    }
}

static int read_response_body(foggy_http_client_t *client, const message_t *message,
                              foggy_http_response_t *response, foggy_http_receiver_t receiver,
                              void *context, int64_t deadline) {
    if (message->status == 204 || message->status == 304) return 0;
    if (message->chunked) return read_chunks(client->connection, &client->input, response, receiver, context);
    if (message->has_length)
        return read_payload(client->connection, &client->input, &response->body,
                            message->length, receiver, context, deadline);
    /* A response without framing is delimited by EOF and cannot be reused. */
    for (;;) {
        if (client->input.size) {
            if (deliver(&response->body, client->input.data, client->input.size, receiver, context) < 0)
                return -1;
            consume(&client->input, client->input.size);
        }
        int n = read_more(client->connection, &client->input, deadline);
        if (n <= 0) return n < 0 ? -1 : 0;
    }
}

static int request(foggy_http_client_t *client, const char *method, const char *path,
                   const char *content_type, const void *body, size_t body_size, foggy_http_receiver_t receiver,
                   void *context, foggy_http_response_t *response) {
    if (!client || !client->host || !path || !*path || !response || (!body && body_size) ||
        body_size > HTTP_BODY_LIMIT) { errno = EINVAL; return -1; }
    for (const char *p = path; *p; ++p)
        if ((unsigned char)*p <= 32 || (unsigned char)*p == 127) { errno = EINVAL; return -1; }
    if (!header_value(client->host)) { errno = EINVAL; return -1; }
    int64_t last = (int64_t)client->last_used.tv_sec * 1000 + client->last_used.tv_nsec / 1000000;
    /* The server expires at three seconds. Reconnect before reusing that idle session. */
    if (client->connection && (!client->keep_alive || milliseconds() - last >= HTTP_IDLE_MS))
        foggy_http_client_close(client);
    foggy_http_buffer_t wire = {0};
    char tail[256];
    snprintf(tail, sizeof(tail), ":%d\r\nConnection: %s\r\n%sContent-Length: %zu\r\n\r\n",
             client->port, client->keep_alive ? "keep-alive" : "close",
             content_type ? "Content-Type: application/json\r\n" : "", body_size);
    const char *version = client->keep_alive ? " HTTP/1.1\r\nHost: " : " HTTP/1.0\r\nHost: ";
    if (foggy_http_buffer_append(&wire, method, strlen(method)) < 0 ||
        foggy_http_buffer_append(&wire, " ", 1) < 0 ||
        foggy_http_buffer_append(&wire, path, strlen(path)) < 0 ||
        foggy_http_buffer_append(&wire, version, strlen(version)) < 0 ||
        foggy_http_buffer_append(&wire, client->host, strlen(client->host)) < 0 ||
        foggy_http_buffer_append(&wire, tail, strlen(tail)) < 0) goto fail;
    if (wire.size > HTTP_HEAD_LIMIT || strlen(path) + strlen(method) + 10 > HTTP_LINE_LIMIT)
        { errno = EINVAL; goto fail; }
    for (int attempt = 0; attempt < 2; ++attempt) {
        int reused = client->connection != NULL;
        if (!client->connection) {
            client->connection = foggy_transport_open(client->host, client->port, 0);
            if (!client->connection) goto fail;
        }
        if (foggy_transport_write(client->connection, wire.data, wire.size) < 0 ||
            foggy_transport_write(client->connection, body, body_size) < 0) {
            if (!attempt && reused && !strcmp(method, "GET")) {
                foggy_http_client_close(client);
                continue;
            }
            goto fail;
        }
        message_t message = {0};
        int64_t deadline = milliseconds() + HTTP_READ_MS;
        int got = read_header(client->connection, &client->input, &message, 1, deadline);
        if (!got && !attempt && reused && !strcmp(method, "GET")) {
            message_free(&message);
            foggy_http_client_close(client);
            continue;
        }
        int interim = 0;
        while (got > 0 && message.status < 200) {
            message_free(&message);
            if (++interim > 16) { got = protocol_error(); break; }
            got = read_header(client->connection, &client->input, &message, 1, deadline);
        }
        if (got <= 0) {
            if (!got) errno = EPROTO;
            message_free(&message);
            goto fail;
        }
        response->status = message.status;
        response->headers = message.headers;
        message.headers = NULL;
        int result = read_response_body(client, &message, response, receiver, context, deadline);
        int keep = client->keep_alive && message.persistent &&
                   (message.has_length || message.chunked || message.status == 204 || message.status == 304);
        message_free(&message);
        if (result < 0) goto fail;
        if (clock_gettime(CLOCK_MONOTONIC, &client->last_used) < 0) goto fail;
        if (!keep) foggy_http_client_close(client);
        foggy_http_buffer_free(&wire);
        return 0;
    }
    errno = EPIPE;
fail: {
    int saved = errno;
    foggy_http_buffer_free(&wire);
    foggy_http_client_close(client);
    errno = saved;
    return -1;
}
}

int foggy_http_client_get(foggy_http_client_t *client, const char *path, foggy_http_response_t *response) {
    return request(client, "GET", path, NULL, NULL, 0, NULL, NULL, response);
}

int foggy_http_client_post(foggy_http_client_t *client, const char *path,
                           const void *body, size_t body_size,
                           foggy_http_receiver_t receiver, void *context,
                           foggy_http_response_t *response) {
    return request(client, "POST", path, NULL, body, body_size, receiver, context, response);
}

int foggy_http_client_post_json(foggy_http_client_t *client, const char *path,
                                const void *body, size_t body_size,
                                foggy_http_receiver_t receiver, void *context,
                                foggy_http_response_t *response) {
    return request(client, "POST", path, "application/json", body, body_size, receiver, context, response);
}

static const char *reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Status";
    }
}

static int write_piece(foggy_transport_t io, const void *data, size_t size, int chunked) {
    if (!size) return 0;
    if (chunked) {
        char line[32];
        int n = snprintf(line, sizeof(line), "%zx\r\n", size);
        if (foggy_transport_write(io, line, (size_t)n) < 0) return -1;
    }
    if (foggy_transport_write(io, data, size) < 0) return -1;
    return chunked ? foggy_transport_write(io, "\r\n", 2) : 0;
}

static int write_response(foggy_transport_t io, const char *version,
                          foggy_http_response_t *response, int *persistent) {
    if (response->status < 200 || response->status > 599 || response->body.size > HTTP_BODY_LIMIT)
        return protocol_error();
    int no_body = response->status == 204 || response->status == 304;
    int streaming = response->next && !no_body;
    int chunked = streaming && !strcmp(version, "HTTP/1.1");
    if ((streaming && !chunked) ||
        contains_token(foggy_http_header_get(response->headers, "Connection"), "close")) *persistent = 0;
    foggy_http_buffer_t wire = {0};
    char line[192];
    int n = snprintf(line, sizeof(line), "%s %d %s\r\n", version, response->status, reason(response->status));
    if (foggy_http_buffer_append(&wire, line, (size_t)n) < 0) goto fail;
    for (foggy_http_header_t *item = response->headers; item; item = item->next) {
        if (!token(item->name) || !header_value(item->value)) { protocol_error(); goto fail; }
        if (!strcasecmp(item->name, "Content-Length") || !strcasecmp(item->name, "Transfer-Encoding") ||
            !strcasecmp(item->name, "Connection")) continue;
        if (strlen(item->name) + strlen(item->value) + 2 > HTTP_LINE_LIMIT) { protocol_error(); goto fail; }
        if (foggy_http_buffer_append(&wire, item->name, strlen(item->name)) < 0 ||
            foggy_http_buffer_append(&wire, ": ", 2) < 0 ||
            foggy_http_buffer_append(&wire, item->value, strlen(item->value)) < 0 ||
            foggy_http_buffer_append(&wire, "\r\n", 2) < 0) goto fail;
    }
    if (chunked) {
        if (foggy_http_buffer_append(&wire, "Transfer-Encoding: chunked\r\n", 28) < 0) goto fail;
    } else if (!streaming) {
        n = snprintf(line, sizeof(line), "Content-Length: %zu\r\n", no_body ? (size_t)0 : response->body.size);
        if (foggy_http_buffer_append(&wire, line, (size_t)n) < 0) goto fail;
    }
    n = snprintf(line, sizeof(line), "Connection: %s\r\n\r\n", *persistent ? "keep-alive" : "close");
    if (foggy_http_buffer_append(&wire, line, (size_t)n) < 0) goto fail;
    if (wire.size > HTTP_HEAD_LIMIT) { protocol_error(); goto fail; }
    if (foggy_transport_write(io, wire.data, wire.size) < 0) goto fail;
    foggy_http_buffer_free(&wire);
    if (no_body) return 0;
    if (write_piece(io, response->body.data, response->body.size, chunked) < 0) return -1;
    size_t total = response->body.size;
    if (streaming) {
        int more;
        do {
            foggy_http_buffer_t part = {0};
            more = response->next(response->next_context, &part);
            int result = 0;
            if (more < 0 || more > 1 || part.size > HTTP_BODY_LIMIT - total) result = protocol_error();
            else {
                total += part.size;
                result = write_piece(io, part.data, part.size, chunked);
            }
            foggy_http_buffer_free(&part);
            if (result < 0) return -1;
        } while (more);
    }
    return chunked ? foggy_transport_write(io, "0\r\n\r\n", 5) : 0;
fail:
    foggy_http_buffer_free(&wire);
    return -1;
}

static int dispatch_exact(const foggy_http_server_t *server, const foggy_http_request_t *request,
                          foggy_http_response_t *response) {
    const foggy_http_route_t *route = server->routes;
    while (route && (strcmp(route->method, request->method) || strcmp(route->path, request->target)))
        route = route->next;
    if (route) return route->handler(request, response, route->context);
    response->status = 404;
    return foggy_http_buffer_append(&response->body, "Not Found\n", 10);
}

int foggy_http_server_listen(foggy_http_server_t *server, int port, int session_limit) {
    return foggy_http_server_listen_with_dispatch(server, port, session_limit, dispatch_exact);
}

int foggy_http_server_listen_with_dispatch(foggy_http_server_t *server, int port, int session_limit,
                                           foggy_http_dispatch_t dispatch) {
    if (!server || !dispatch || port < 1 || port > 65535 || session_limit < 0) {
        errno = EINVAL;
        return -1;
    }
    for (int session = 0; session_limit == 0 || session < session_limit; ++session) {
        foggy_transport_t io = foggy_transport_open("0.0.0.0", port, 1);
        if (!io) return -1;
        foggy_http_buffer_t input = {0};
        int idle_ms = HTTP_READ_MS;
        for (;;) {
            if (!input.size && read_more(io, &input, milliseconds() + idle_ms) <= 0) break;
            int64_t deadline = milliseconds() + HTTP_READ_MS;
            message_t message = {0};
            int got = read_header(io, &input, &message, 0, deadline);
            if (got <= 0) { message_free(&message); break; }
            foggy_http_buffer_t body = {0};
            if (read_payload(io, &input, &body, message.length, NULL, NULL, deadline) < 0) {
                foggy_http_buffer_free(&body);
                message_free(&message);
                break;
            }
            foggy_http_request_t request = {message.method, message.target, message.version,
                                           message.headers, body.data, body.size};
            foggy_http_response_t response;
            foggy_http_response_init(&response);
            int result = dispatch(server, &request, &response);
            int persistent = message.persistent;
            if (result == 0) result = write_response(io, message.version, &response, &persistent);
            foggy_http_response_free(&response);
            foggy_http_buffer_free(&body);
            message_free(&message);
            if (result < 0 || !persistent) break;
            idle_ms = HTTP_IDLE_MS;
        }
        foggy_http_buffer_free(&input);
        foggy_transport_close(&io);
    }
    return 0;
}
