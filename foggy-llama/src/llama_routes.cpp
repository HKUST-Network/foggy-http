#include "api.h"
#include <cerrno>

namespace foggy_llama {
namespace {
// Convert handler failures to JSON responses.
int failure(foggy_http_response_t *response, const char *message, int status) noexcept {
    try { return error_response(response, message, status); }
    catch (...) { errno = ENOMEM; return -1; }
}
}

int health(const foggy_http_request_t *, foggy_http_response_t *response, void *context) noexcept {
    // TODO: Report the model ID from ServiceContext, or return 503.
    (void)context;
    return failure(response, "TODO: health handler", 500);
}

int models(const foggy_http_request_t *, foggy_http_response_t *response, void *context) noexcept {
    // TODO: Return the engine metadata as a JSON list.
    (void)context;
    return failure(response, "TODO: model list handler", 500);
}

int chat(const foggy_http_request_t *request, foggy_http_response_t *response, void *context) noexcept {
    // TODO: Validate, check the rate limit, then start streaming.
    // Use failure() for validation and engine errors.
    (void)request;
    (void)context;
    return failure(response, "TODO: chat handler", 500);
}
}
