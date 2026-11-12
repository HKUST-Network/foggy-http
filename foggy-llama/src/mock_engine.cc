#include "engine.h"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace foggy_llama {
namespace {
struct MockEngine {
    std::string alias;
    int delay_ms;
};

struct MockGeneration {
    std::string text;
    size_t at;
    int delay_ms;
};

bool mock_next(void *context, std::string &text, const char **finish_reason) {
    auto &state = *static_cast<MockGeneration *>(context);
    *finish_reason = "stop";
    text.clear();
    if (state.at == state.text.size()) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(state.delay_ms));
    // Keep UTF-8 characters intact; HTTP and receiver fragments may still split them.
    const size_t start = state.at;
    for (int chars = 0; chars < 3 && state.at < state.text.size(); ++chars) {
        const auto first = static_cast<unsigned char>(state.text[state.at]);
        state.at += first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        if (state.at > state.text.size()) state.at = state.text.size();
    }
    text = state.text.substr(start, state.at - start);
    return state.at < state.text.size();
}

Json mock_model_info(const void *context) {
    const auto &state = *static_cast<const MockEngine *>(context);
    return {{"id", state.alias}, {"object", "model"}, {"owned_by", "mock"}};
}

Generation mock_start_chat(void *context, const Chat &chat) {
    const auto &state = *static_cast<const MockEngine *>(context);
    if (chat.messages.empty()) throw std::invalid_argument("messages are empty");
    auto generation = std::make_shared<MockGeneration>(MockGeneration{
        "mock messages=" + std::to_string(chat.messages.size()) + ": " + chat.messages.back().content,
        0, state.delay_ms});
    return {generation, mock_next};
}
}

Engine make_mock_engine(const std::string &alias, int delay_ms) {
    if (alias.empty() || delay_ms < 0) throw std::invalid_argument("invalid mock configuration");
    return {std::make_shared<MockEngine>(MockEngine{alias, delay_ms}), mock_model_info, mock_start_chat};
}
}
