#include "api.h"
#include <cerrno>

namespace foggy_llama {
int register_routes(foggy_http_server_t *server, ServiceContext &service) noexcept {
    // TODO: Bind the provided handlers to the three routes in the handout.
    // Pass &service as their context. Return 0 on success or -1 with errno on failure.
    (void)server;
    (void)service;
    errno = ENOSYS;
    return -1;
}
}
