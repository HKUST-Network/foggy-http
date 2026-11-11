#include "client_flow.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace foggy_llama {
namespace {
using Clock = std::chrono::steady_clock;

class Response {
public:
    foggy_http_response_t value;
    Response() { foggy_http_response_init(&value); }
    ~Response() { foggy_http_response_free(&value); }
    Response(const Response &) = delete;
    Response &operator=(const Response &) = delete;
};

std::string body(const foggy_http_response_t &response) {
    return response.body.size ? std::string(response.body.data, response.body.size) : std::string();
}

void require_success(const foggy_http_response_t &response) {
    if (response.status != 200)
        throw std::runtime_error("HTTP " + std::to_string(response.status) + ": " + body(response));
}

void report(foggy_http_client_t *client, const foggy_http_response_t &response) {
    std::cerr << "RESPONSE " << response.status << " local_port="
              << foggy_http_client_connection_port(client) << std::endl;
}

bool is_event_stream(const foggy_http_response_t &response) {
    const char *type = foggy_http_header_get(response.headers, "Content-Type");
    if (!type) return false;
    std::string value(type);
    auto end = value.find(';');
    value.resize(end == std::string::npos ? value.size() : end);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value == "text/event-stream";
}

struct Receiver {
    Events *events;
    foggy_http_response_t *response;
    std::exception_ptr error;
};

// C callbacks must never unwind into the C HTTP codec. Keep draining after an
// application error; report it once the HTTP call returns to C++.
void receive(const void *data, size_t size, void *context) noexcept {
    auto &state = *static_cast<Receiver *>(context);
    if (state.error || state.response->status != 200) return;
    try {
        if (!is_event_stream(*state.response)) throw std::runtime_error("expected text/event-stream");
        state.events->feed(std::string(static_cast<const char *>(data), size));
    } catch (...) {
        state.error = std::current_exception();
    }
}
Json get_json(foggy_http_client_t *client, const char *path, const char *failure) {
    if (!client) throw std::invalid_argument("missing HTTP client");
    Response response;
    if (foggy_http_client_get(client, path, &response.value) < 0)
        throw std::runtime_error(std::string(failure) + ": " + std::strerror(errno));
    report(client, response.value);
    require_success(response.value);
    return Json::parse(body(response.value));
}
}

Json health_status(foggy_http_client_t *client) {
    return get_json(client, "/health", "health check failed");
}

Json discover_models(foggy_http_client_t *client) {
    auto models = get_json(client, "/v1/models", "model discovery failed");
    if (!models.is_object() || models.value("object", "") != "list" ||
        !models.contains("data") || !models["data"].is_array() || models["data"].empty())
        throw std::runtime_error("invalid model list");
    for (const auto &model : models["data"])
        if (!model.is_object() || !model.contains("id") || !model["id"].is_string() ||
            model["id"].get<std::string>().empty()) throw std::runtime_error("invalid model id");
    return models;
}

std::string discover_model(const Json &models, const std::string &requested) {
    const auto &data = models.at("data");
    if (!data.is_array() || data.empty()) throw std::runtime_error("no models available");
    if (requested.empty()) return data.at(0).at("id").get<std::string>();
    for (const auto &model : data)
        if (model.at("id") == requested) return requested;
    throw std::invalid_argument("unknown model " + requested);
}

TurnResult chat_turn(foggy_http_client_t *client, const std::string &model,
                     Json &history, const std::string &prompt, int max_tokens,
                     std::function<void(const std::string &)> output) {
    if (!client || model.empty() || !history.is_array() || max_tokens < 1 || max_tokens > 128)
        throw std::invalid_argument("invalid chat configuration");
    Json messages = history;
    messages.push_back({{"role", "user"}, {"content", prompt}});
    const auto wire = Json({{"model", model}, {"messages", messages},
                            {"max_tokens", max_tokens}, {"stream", true}}).dump();
    TurnResult result;
    const auto start = Clock::now();
    Events events([&](const std::string &text) {
        if (text.empty()) return;
        if (result.first_ms < 0) result.first_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count();
        result.text += text;
        if (output) output(text);
    });
    Response response;
    Receiver receiver{&events, &response.value, {}};
    int status = foggy_http_client_post(client, "/v1/chat/completions", wire.data(), wire.size(),
                                             receive, &receiver, &response.value);
    int saved = errno;
    if (receiver.error) std::rethrow_exception(receiver.error);
    if (status < 0) throw std::runtime_error(std::string("chat request failed: ") + std::strerror(saved));
    report(client, response.value);
    require_success(response.value);
    if (!is_event_stream(response.value)) throw std::runtime_error("expected text/event-stream");
    events.finish();
    messages.push_back({{"role", "assistant"}, {"content", result.text}});
    history = std::move(messages);
    result.messages = history.size();
    result.total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
    return result;
}
}
