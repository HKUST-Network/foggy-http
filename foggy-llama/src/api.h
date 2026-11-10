#pragma once
#include "engine.h"
#include "foggy_http.h"
#include "rate_limiter.h"
#include <functional>

namespace foggy_llama {
// Responses must be initialized first. These helpers return 0 or -1 with errno.
int json_response(foggy_http_response_t *response, const Json &value, int status = 200);
int error_response(foggy_http_response_t *response, const std::string &message, int status);
Chat parse_chat(const foggy_http_request_t &request, const Json &model_info);
int chat_response(foggy_http_response_t *response, std::shared_ptr<Generation> generation,
                  const std::string &model);
struct ServiceContext {
    Engine &engine;
    ChatRateLimiter limiter{};
    std::function<ChatRateLimiter::Clock::time_point()> now = ChatRateLimiter::Clock::now;
};
// The service context and engine are borrowed and must outlive the server.
int register_llama_routes(foggy_http_server_t *server, ServiceContext &service);
int dispatch_request(const foggy_http_server_t *server, const foggy_http_request_t *request,
                     foggy_http_response_t *response) noexcept;

// Decoder for this service's SSE subset; read/chunk boundaries are arbitrary.
class Events {
public:
    explicit Events(std::function<void(const std::string &)> output);
    void feed(const std::string &data);
    void finish() const;
    bool done() const { return done_; }
private:
    std::function<void(const std::string &)> output_;
    std::string buffer_;
    bool done_ = false, finished_ = false;
};
}
