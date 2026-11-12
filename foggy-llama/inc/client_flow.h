#pragma once
#include "api.h"
#include <cstddef>
#include <functional>
#include <string>

namespace foggy_llama {
struct TurnResult {
    std::string text;
    size_t messages = 0;
    long long first_ms = -1, total_ms = 0;
};

// The caller owns the initialized Foggy HTTP client and conversation history.
Json health_status(foggy_http_client_t *client);
Json discover_models(foggy_http_client_t *client);
std::string discover_model(const Json &models, const std::string &requested = "");
// Commit the user/assistant pair only after a complete, successful SSE response.
TurnResult chat_turn(foggy_http_client_t *client, const std::string &model,
                     Json &history, const std::string &prompt, int max_tokens,
                     std::function<void(const std::string &)> output);
}
