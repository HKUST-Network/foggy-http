#pragma once
#include "json.hpp"
#include <memory>
#include <string>
#include <vector>

namespace foggy_llama {
using Json = nlohmann::ordered_json;
struct Message { std::string role, content; };
struct Chat { std::string model; std::vector<Message> messages; int max_tokens = 64; };
struct Generation {
    std::shared_ptr<void> context;
    // The final call returns false and sets finish_reason. It may also return text.
    bool (*next)(void *context, std::string &text, const char **finish_reason);
};

struct Engine {
    std::shared_ptr<void> context;
    Json (*read_model_info)(const void *context);
    Generation (*start_chat)(void *context, const Chat &chat);
};

inline Json engine_model_info(const Engine *engine) {
    return engine->read_model_info(engine->context.get());
}

inline Generation engine_start_chat(Engine *engine, const Chat &chat) {
    return engine->start_chat(engine->context.get(), chat);
}

Engine make_mock_engine(const std::string &alias, int delay_ms = 100);
Engine make_llama_engine(const std::string &file, const std::string &alias,
                         int context = 2048, int threads = 2, int gpu_layers = 0);
}
