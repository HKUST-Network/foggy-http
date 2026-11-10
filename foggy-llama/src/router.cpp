#include "api.h"
#include <cerrno>

namespace foggy_llama {
int dispatch_request(const foggy_http_server_t *server, const foggy_http_request_t *request,
                     foggy_http_response_t *response) noexcept {
    try {
        // TODO: Match method and path, ignoring the query string.
        // Call the handler or return a JSON 404 or 405.
        (void)server;
        (void)request;
        return error_response(response, "TODO: dispatch request", 500);
    } catch (...) {
        errno = EIO;
        return -1;
    }
}
}
