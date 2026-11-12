#ifndef FOGGY_LLAMA_H
#define FOGGY_LLAMA_H

#include "foggy_http.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct foggy_llama_service foggy_llama_service_t;

/* The server borrows service as the context for all three routes. */
int foggy_llama_register_routes(foggy_http_server_t *server, foggy_llama_service_t *service);

int foggy_llama_health(const foggy_http_request_t *request, foggy_http_response_t *response, void *context);
int foggy_llama_models(const foggy_http_request_t *request, foggy_http_response_t *response, void *context);
int foggy_llama_chat(const foggy_http_request_t *request, foggy_http_response_t *response, void *context);

#ifdef __cplusplus
}
#endif

#endif
