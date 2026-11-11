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
    typedef std::function<ChatRateLimiter::Clock::time_point()> ClockFunction;

    explicit ServiceContext(Engine &model_engine, ClockFunction clock = ChatRateLimiter::Clock::now)
        : engine(model_engine), now(clock) {}

    Engine &engine;
    ChatRateLimiter limiter;
    ClockFunction now;
};
// The service context is borrowed and must outlive the server.
int health(const foggy_http_request_t *request, foggy_http_response_t *response, void *context) noexcept;
int models(const foggy_http_request_t *request, foggy_http_response_t *response, void *context) noexcept;
int chat(const foggy_http_request_t *request, foggy_http_response_t *response, void *context) noexcept;

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
