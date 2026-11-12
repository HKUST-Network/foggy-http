#pragma once
#include "engine.h"
#include "foggy_llama.h"
#include <functional>

struct foggy_llama_service {
    foggy_llama::Engine *engine;
};

namespace foggy_llama {
// Responses must be initialized first. These helpers return 0 or -1 with errno.
int json_response(foggy_http_response_t *response, const Json &value, int status = 200);
int error_response(foggy_http_response_t *response, const std::string &message, int status);
Chat parse_chat(const foggy_http_request_t &request, const Json &model_info);
int chat_response(foggy_http_response_t *response, Generation generation,
                  const std::string &model);

// Decoder for this service's SSE subset; read/chunk boundaries are arbitrary.
struct Events {
    std::function<void(const std::string &)> output;
    std::string buffer;
    bool done, finished;
};
void events_init(Events *events, std::function<void(const std::string &)> output);
void events_feed(Events *events, const std::string &data);
void events_finish(const Events *events);
}
