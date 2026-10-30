/* Fixed course support: inspect the first FoggyTCP UDP datagram without consuming it.
 * A complete initial HTTP request line is required; the selected upstream is
 * fixed for the UDP session. Transport reliability remains at the endpoints. */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>

#define FOGGY_PREFIX 25
#define FOGGY_IDENTIFIER 3120
#define HTTP_LINE_LIMIT 8192

static ngx_uint_t
read_u16(const u_char *p)
{
    return ((ngx_uint_t) p[0] << 8) | p[1];
}

static uint32_t
read_u32(const u_char *p)
{
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16)
        | ((uint32_t) p[2] << 8) | p[3];
}

static ngx_int_t
reject_packet(ngx_stream_session_t *s, ngx_stream_variable_value_t *v,
    const char *reason)
{
    v->not_found = 1;
    v->valid = 0;
    ngx_log_error(NGX_LOG_INFO, s->connection->log, 0,
                  "FOGGY_REJECT %s", reason);
    return NGX_OK;
}

static ngx_int_t
path_value(ngx_stream_session_t *s, ngx_stream_variable_value_t *v,
    uintptr_t data)
{
    ngx_buf_t *b;
    u_char *packet, *payload, *end, *cr, *target, *target_end, *version, *q;
    size_t n, hlen, plen, method_len, path_len;
    (void) data;

    b = s->connection->buffer;
    if (s->connection->type != SOCK_DGRAM || b == NULL) {
        return reject_packet(s, v, "UDP datagram required");
    }
    packet = b->pos;
    n = b->last - b->pos;
    if (n < FOGGY_PREFIX || read_u32(packet) != FOGGY_IDENTIFIER) {
        return reject_packet(s, v, "invalid identifier or truncated header");
    }
    hlen = read_u16(packet + 16);
    plen = read_u16(packet + 18);
    if (hlen < FOGGY_PREFIX || hlen >= plen || plen != n
        || read_u16(packet + 23) > hlen - FOGGY_PREFIX)
    {
        return reject_packet(s, v, "invalid header, packet or extension length");
    }
    if (read_u32(packet + 8) != 0 || (packet[20] & 0x0a)) {
        return reject_packet(s, v, "initial sequence-zero data required");
    }

    payload = packet + hlen;
    end = packet + plen;
    cr = payload;
    while (cr < end && (size_t) (cr - payload) <= HTTP_LINE_LIMIT) {
        if (*cr == '\r') break;
        if (*cr < 0x20 || *cr == 0x7f) {
            return reject_packet(s, v, "invalid request-line character");
        }
        cr++;
    }
    if (cr == end || (size_t) (cr - payload) > HTTP_LINE_LIMIT
        || end - cr < 2 || cr[1] != '\n')
    {
        return reject_packet(s, v, "complete HTTP request line required");
    }

    q = ngx_strlchr(payload, cr, ' ');
    if (q == NULL) return reject_packet(s, v, "missing method separator");
    method_len = q - payload;
    if (!((method_len == 3 && ngx_memcmp(payload, "GET", 3) == 0)
          || (method_len == 4 && ngx_memcmp(payload, "POST", 4) == 0)))
    {
        return reject_packet(s, v, "unsupported method");
    }
    target = q + 1;
    target_end = ngx_strlchr(target, cr, ' ');
    if (target_end == NULL || target == target_end || *target != '/') {
        return reject_packet(s, v, "invalid request target");
    }
    version = target_end + 1;
    if (cr - version != 8
        || (ngx_memcmp(version, "HTTP/1.1", 8) != 0
            && ngx_memcmp(version, "HTTP/1.0", 8) != 0))
    {
        return reject_packet(s, v, "invalid HTTP version");
    }
    q = ngx_strlchr(target, target_end, '?');
    if (q != NULL) target_end = q;
    path_len = target_end - target;
    v->data = ngx_pnalloc(s->connection->pool, path_len);
    if (v->data == NULL) return NGX_ERROR;
    ngx_memcpy(v->data, target, path_len);
    v->len = path_len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    ngx_log_error(NGX_LOG_INFO, s->connection->log, 0,
                  "FOGGY_PATH %v", v);
    return NGX_OK;
}

static ngx_int_t
add_variables(ngx_conf_t *cf)
{
    ngx_stream_variable_t *v;
    ngx_str_t name = ngx_string("foggy_http_path");
    v = ngx_stream_add_variable(cf, &name, 0);
    if (v == NULL) return NGX_ERROR;
    v->get_handler = path_value;
    return NGX_OK;
}

static ngx_stream_module_t module_context = {
    add_variables, NULL, NULL, NULL, NULL, NULL
};

ngx_module_t ngx_stream_foggy_http_module = {
    NGX_MODULE_V1, &module_context, NULL, NGX_STREAM_MODULE,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NGX_MODULE_V1_PADDING
};
