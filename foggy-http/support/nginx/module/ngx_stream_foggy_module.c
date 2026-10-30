/* Fixed course adapter: expose the preread datagram without consuming it.
 * Protocol decoding and routing belong to Lua; this module does neither. */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>

static ngx_int_t packet_value(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    ngx_buf_t *b = s->connection->buffer;
    (void)data;
    if (s->connection->type != SOCK_DGRAM || b == NULL || b->last <= b->pos) {
        v->not_found = 1;
        return NGX_OK;
    }
    v->len = b->last - b->pos;
    v->data = b->pos;
    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;
}

static ngx_int_t backend_value(ngx_stream_session_t *s,
    ngx_stream_variable_value_t *v, uintptr_t data)
{
    (void)s;
    (void)data;
    v->not_found = 1;
    return NGX_OK;
}

static ngx_int_t add_variables(ngx_conf_t *cf)
{
    ngx_stream_variable_t *v;
    ngx_str_t packet = ngx_string("foggy_packet");
    ngx_str_t backend = ngx_string("foggy_backend");
    v = ngx_stream_add_variable(cf, &packet, NGX_STREAM_VAR_NOCACHEABLE);
    if (v == NULL) return NGX_ERROR;
    v->get_handler = packet_value;
    v = ngx_stream_add_variable(cf, &backend, NGX_STREAM_VAR_CHANGEABLE);
    if (v == NULL) return NGX_ERROR;
    v->get_handler = backend_value;
    return ngx_stream_get_variable_index(cf, &backend) == NGX_ERROR
        ? NGX_ERROR : NGX_OK;
}

static ngx_stream_module_t module_context = {
    add_variables, NULL, NULL, NULL, NULL, NULL
};

ngx_module_t ngx_stream_foggy_module = {
    NGX_MODULE_V1, &module_context, NULL, NGX_STREAM_MODULE,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NGX_MODULE_V1_PADDING
};
