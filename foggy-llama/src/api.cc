#include "api.h"
#include <atomic>
#include <cerrno>
#include <ctime>
#include <new>
#include <stdexcept>
#include <utility>

namespace {
int exception_errno() noexcept {
    try { throw; }
    catch (const std::bad_alloc &) { errno = ENOMEM; }
    catch (...) { errno = EINVAL; }
    return -1;
}

int handler_error(foggy_http_response_t *response, const char *message, int status) noexcept {
    try { return foggy_llama::error_response(response, message, status); }
    catch (...) { return exception_errno(); }
}
}

namespace foggy_llama {
namespace {
struct Stream {
    Generation generation;
    std::string model, id;
    std::time_t created;
    bool ended;
};

std::string stream_event(const Stream *stream, const Json &delta, const Json &reason) {
    return "data: " + Json({{"id", stream->id}, {"object", "chat.completion.chunk"},
        {"created", stream->created}, {"model", stream->model}, {"choices", Json::array({
            {{"index", 0}, {"delta", delta}, {"finish_reason", reason}}})}}).dump() + "\n\n";
}

void free_stream(void *context) noexcept { delete static_cast<Stream *>(context); }

int stream_error(foggy_http_buffer_t *output, const char *message) noexcept {
    try {
        const auto wire = "data: " + Json({{"error", {{"message", message}}}}).dump() + "\n\n";
        return foggy_http_buffer_append(output, wire.data(), wire.size()) < 0 ? -1 : 0;
    } catch (...) { return exception_errno(); }
}

int next_stream(void *context, foggy_http_buffer_t *output) noexcept {
    auto *stream = static_cast<Stream *>(context);
    if (!stream || !output) { errno = EINVAL; return -1; }
    if (stream->ended) return 0;
    try {
        std::string text, wire;
        const char *finish_reason = nullptr;
        auto &generation = stream->generation;
        bool more = generation.next(generation.context.get(), text, &finish_reason);
        if (!text.empty()) wire += stream_event(stream, {{"content", text}}, nullptr);
        if (!more) {
            wire += stream_event(stream, Json::object(), finish_reason);
            wire += "data: [DONE]\n\n";
            stream->ended = true;
        }
        if (foggy_http_buffer_append(output, wire.data(), wire.size()) < 0) return -1;
        return more ? 1 : 0;
    } catch (const std::exception &error) {
        // HTTP headers have been sent. A failed generation must never emit DONE.
        stream->ended = true;
        return stream_error(output, error.what());
    } catch (...) {
        stream->ended = true;
        return stream_error(output, "unknown generation failure");
    }
}
}

int json_response(foggy_http_response_t *response, const Json &value, int status) {
    if (!response) { errno = EINVAL; return -1; }
    foggy_http_response_t result;
    foggy_http_response_init(&result);
    try {
        const auto body = value.dump();
        result.status = status;
        if (foggy_http_header_set(&result.headers, "Content-Type", "application/json; charset=utf-8") < 0 ||
            foggy_http_buffer_append(&result.body, body.data(), body.size()) < 0) {
            const int saved = errno;
            foggy_http_response_free(&result);
            errno = saved;
            return -1;
        }
        foggy_http_response_free(response);
        *response = result;
        return 0;
    } catch (...) {
        foggy_http_response_free(&result);
        return exception_errno();
    }
}

int error_response(foggy_http_response_t *response, const std::string &message, int status) {
    try { return json_response(response, {{"error", {{"message", message}}}}, status); }
    catch (...) { return exception_errno(); }
}

Chat parse_chat(const foggy_http_request_t &request, const Json &info) {
    if (!request.body && request.body_size) throw std::invalid_argument("missing request body");
    try {
        const auto value = Json::parse(request.body_size ?
            std::string(static_cast<const char *>(request.body), request.body_size) : std::string());
        if (!value.is_object() || !value.contains("model") || !value["model"].is_string() ||
            value["model"] != info.at("id")) throw std::invalid_argument("unknown model");
        if (value.contains("stream") && (!value["stream"].is_boolean() || !value["stream"].get<bool>()))
            throw std::invalid_argument("only stream=true is supported");
        Chat chat;
        chat.model = value["model"].get<std::string>();
        if (value.contains("max_tokens")) {
            if (!value["max_tokens"].is_number_integer() || value["max_tokens"] < 1 || value["max_tokens"] > 128)
                throw std::invalid_argument("max_tokens must be an integer in 1..128");
            chat.max_tokens = value["max_tokens"].get<int>();
        }
        if (!value.contains("messages") || !value["messages"].is_array() ||
            value["messages"].empty() || value["messages"].size() > 64)
            throw std::invalid_argument("messages must contain 1..64 text messages");
        size_t bytes = 0;
        for (const auto &item : value["messages"]) {
            if (!item.is_object() || !item.contains("role") || !item["role"].is_string() ||
                !item.contains("content") || !item["content"].is_string())
                throw std::invalid_argument("message requires string role/content");
            Message message{item["role"].get<std::string>(), item["content"].get<std::string>()};
            if (message.role != "system" && message.role != "user" && message.role != "assistant")
                throw std::invalid_argument("unsupported role");
            if (message.content.find('\0') != std::string::npos) throw std::invalid_argument("NUL in content");
            bytes += message.content.size();
            if (bytes > 16384) throw std::invalid_argument("messages exceed 16 KiB");
            chat.messages.push_back(std::move(message));
        }
        if (chat.messages.back().role != "user") throw std::invalid_argument("last message must be user");
        return chat;
    } catch (const nlohmann::json::exception &) {
        throw std::invalid_argument("invalid chat JSON");
    }
}

int chat_response(foggy_http_response_t *response, Generation generation,
                  const std::string &model) {
    if (!response || !generation.context || !generation.next) { errno = EINVAL; return -1; }
    foggy_http_response_t result;
    foggy_http_response_init(&result);
    try {
        static std::atomic<unsigned long long> serial{0};
        std::unique_ptr<Stream> stream(new Stream{std::move(generation), model,
            "chatcmpl-foggy-" + std::to_string(++serial), std::time(nullptr), false});
        if (foggy_http_header_set(&result.headers, "Content-Type", "text/event-stream") < 0 ||
            foggy_http_header_set(&result.headers, "Cache-Control", "no-cache") < 0) {
            const int saved = errno;
            foggy_http_response_free(&result);
            errno = saved;
            return -1;
        }
        result.next = next_stream;
        result.next_context = stream.release();
        result.free_context = free_stream;
        foggy_http_response_free(response);
        *response = result;
        return 0;
    } catch (...) {
        foggy_http_response_free(&result);
        return exception_errno();
    }
}

void events_init(Events *events, std::function<void(const std::string &)> output) {
    *events = Events{std::move(output), {}, false, false};
}

void events_feed(Events *events, const std::string &data) {
    events->buffer += data;
    if (events->buffer.size() > 1024 * 1024) throw std::runtime_error("SSE event too large");
    for (;;) {
        const auto end = events->buffer.find("\n\n");
        if (end == std::string::npos) return;
        const auto event = events->buffer.substr(0, end);
        events->buffer.erase(0, end + 2);
        if (event.empty() || event[0] == ':') continue;
        if (events->done || event.rfind("data: ", 0) != 0) throw std::runtime_error("unexpected SSE event");
        const auto payload = event.substr(6);
        if (payload == "[DONE]") {
            if (!events->finished) throw std::runtime_error("DONE without finish_reason");
            events->done = true;
            continue;
        }
        const auto value = Json::parse(payload);
        if (value.contains("error")) throw std::runtime_error(value["error"].dump());
        if (events->finished) throw std::runtime_error("data after finish event");
        const auto choice = value.at("choices").at(0);
        const auto delta = choice.at("delta");
        if (delta.contains("content")) events->output(delta.at("content").get<std::string>());
        if (!choice.at("finish_reason").is_null()) events->finished = true;
    }
}

void events_finish(const Events *events) {
    if (!events->done || !events->finished || !events->buffer.empty())
        throw std::runtime_error("incomplete SSE response");
}
}

int foggy_llama_health(const foggy_http_request_t *, foggy_http_response_t *response, void *context) {
    try {
        const auto *service = static_cast<const foggy_llama_service_t *>(context);
        const auto id = foggy_llama::engine_model_info(service->engine).at("id").get<std::string>();
        return foggy_llama::json_response(response, {{"status", "ok"}, {"model", id}});
    } catch (const std::exception &error) {
        return handler_error(response, error.what(), 503);
    } catch (...) {
        return handler_error(response, "model unavailable", 503);
    }
}

int foggy_llama_models(const foggy_http_request_t *, foggy_http_response_t *response, void *context) {
    try {
        const auto *service = static_cast<const foggy_llama_service_t *>(context);
        const auto info = foggy_llama::engine_model_info(service->engine);
        return foggy_llama::json_response(response, {{"object", "list"}, {"data", foggy_llama::Json::array({info})}});
    } catch (const std::exception &error) {
        return handler_error(response, error.what(), 500);
    } catch (...) {
        return handler_error(response, "model lookup failed", 500);
    }
}

int foggy_llama_chat(const foggy_http_request_t *request, foggy_http_response_t *response, void *context) {
    try {
        const auto *service = static_cast<const foggy_llama_service_t *>(context);
        const auto input = foggy_llama::parse_chat(*request, foggy_llama::engine_model_info(service->engine));
        auto generation = foggy_llama::engine_start_chat(service->engine, input);
        if (!generation.context) return handler_error(response, "inference did not start", 500);
        return foggy_llama::chat_response(response, std::move(generation), input.model);
    } catch (const std::invalid_argument &error) {
        return handler_error(response, error.what(), 400);
    } catch (const std::exception &error) {
        return handler_error(response, error.what(), 500);
    } catch (...) {
        return handler_error(response, "chat failed", 500);
    }
}
