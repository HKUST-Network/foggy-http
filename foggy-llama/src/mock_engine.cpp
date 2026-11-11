#include "engine.h"
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

namespace foggy_llama {
namespace {
class MockGeneration : public Generation {
    std::string text_;
    size_t at_ = 0;
    int delay_;
public:
    MockGeneration(std::string text, int delay) : text_(std::move(text)), delay_(delay) {}
    bool next(std::string &text) override {
        text.clear();
        if (at_ == text_.size()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
        // Keep UTF-8 characters intact; HTTP and receiver fragments may still split them.
        const size_t start = at_;
        for (int chars = 0; chars < 3 && at_ < text_.size(); ++chars) {
            const auto first = static_cast<unsigned char>(text_[at_]);
            at_ += first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
            if (at_ > text_.size()) at_ = text_.size();
        }
        text = text_.substr(start, at_ - start);
        return at_ < text_.size();
    }
    std::string finish_reason() const override { return "stop"; }
};
class MockEngine : public Engine {
    std::string alias_;
    int delay_;
public:
    MockEngine(std::string alias, int delay) : alias_(std::move(alias)), delay_(delay) {}
    Json model_info() const override {
        return {{"id", alias_}, {"object", "model"}, {"owned_by", "mock"}};
    }
    std::shared_ptr<Generation> start_chat(const Chat &chat) override {
        if (chat.messages.empty()) throw std::invalid_argument("messages are empty");
        return std::make_shared<MockGeneration>("mock messages=" + std::to_string(chat.messages.size()) +
            ": " + chat.messages.back().content, delay_);
    }
};
}
std::unique_ptr<Engine> make_mock_engine(const std::string &alias, int delay_ms) {
    if (alias.empty() || delay_ms < 0) throw std::invalid_argument("invalid mock configuration");
    return std::unique_ptr<Engine>(new MockEngine(alias, delay_ms));
}
}
