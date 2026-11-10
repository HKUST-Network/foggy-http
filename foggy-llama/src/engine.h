#pragma once
#include "json.hpp"
#include <memory>
#include <string>
#include <vector>

namespace foggy_llama {
using Json = nlohmann::ordered_json;
struct Message { std::string role, content; };
struct Chat { std::string model; std::vector<Message> messages; int max_tokens = 64; };
class Generation {
public:
    virtual ~Generation() = default;
    // false means finished; this final call may still return text.
    virtual bool next(std::string &text) = 0;
    virtual std::string finish_reason() const = 0;
};
class Engine {
public:
    virtual ~Engine() = default;
    virtual Json model_info() const = 0;
    virtual std::shared_ptr<Generation> start_chat(const Chat &chat) = 0;
};
std::unique_ptr<Engine> make_mock_engine(const std::string &alias, int delay_ms = 100);
std::unique_ptr<Engine> make_llama_engine(const std::string &file, const std::string &alias,
                                        int context = 2048, int threads = 2, int gpu_layers = 0);
}
