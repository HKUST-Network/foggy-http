#include "engine.h"
#include "llama.h"
#include <algorithm>
#include <stdexcept>

namespace foggy_llama {
namespace {
struct Model {
    llama_model *handle = nullptr;
    ~Model() { if (handle) llama_model_free(handle); }
};
// Token pieces may split a UTF-8 codepoint. Emit complete valid codepoints only.
std::string utf8_ready(std::string &pending, bool final) {
    std::string out;
    size_t at = 0;
    while (at < pending.size()) {
        auto first = static_cast<unsigned char>(pending[at]);
        size_t n = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
                   first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (n && at + n > pending.size() && !final) break;
        bool valid = n && at + n <= pending.size();
        unsigned value = n == 1 ? first : first & (0x7f >> n);
        for (size_t i = 1; valid && i < n; ++i) {
            auto byte = static_cast<unsigned char>(pending[at+i]);
            valid = (byte & 0xc0) == 0x80;
            value = (value << 6) | (byte & 0x3f);
        }
        valid = valid && (n == 1 || value >= (n == 2 ? 0x80u : n == 3 ? 0x800u : 0x10000u)) &&
                value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
        if (valid) { out.append(pending, at, n); at += n; }
        else { out += "\xef\xbf\xbd"; ++at; }
    }
    pending.erase(0, at);
    return out;
}
class LlamaGeneration : public Generation {
    std::shared_ptr<Model> model_;
    std::unique_ptr<llama_context, decltype(&llama_free)> context_{nullptr, llama_free};
    std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler_{nullptr, llama_sampler_free};
    std::vector<llama_token> prompt_;
    const llama_vocab *vocab_;
    bool prefilled_ = false, ended_ = false;
    int limit_, generated_ = 0;
    llama_token previous_ = 0;
    std::string reason_ = "stop", pending_;
public:
    LlamaGeneration(std::shared_ptr<Model> model, const Chat &chat, int context_size, int threads)
        : model_(std::move(model)), vocab_(llama_model_get_vocab(model_->handle)), limit_(chat.max_tokens) {
        std::vector<llama_chat_message> messages;
        for (const auto &m : chat.messages) messages.push_back({m.role.c_str(), m.content.c_str()});
        auto tmpl = llama_model_chat_template(model_->handle, nullptr);
        if (!tmpl) throw std::invalid_argument("model has no chat template");
        int size = llama_chat_apply_template(tmpl, messages.data(), messages.size(), true, nullptr, 0);
        if (size < 0 || size > 256*1024) throw std::invalid_argument("unsupported or oversized chat template");
        std::string prompt(size, '\0');
        int written = llama_chat_apply_template(tmpl, messages.data(), messages.size(), true, prompt.data(), prompt.size());
        if (written != size) throw std::runtime_error("chat template changed size");
        int count = llama_tokenize(vocab_, prompt.data(), prompt.size(), nullptr, 0, true, true);
        if (count >= 0) throw std::invalid_argument("empty prompt");
        count = -count;
        if (count + limit_ > context_size) throw std::invalid_argument("prompt plus max_tokens exceeds context");
        prompt_.resize(count);
        if (llama_tokenize(vocab_, prompt.data(), prompt.size(), prompt_.data(), count, true, true) != count)
            throw std::runtime_error("tokenize failed");
        auto params = llama_context_default_params();
        params.n_ctx = context_size;
        params.n_batch = context_size;
        params.n_ubatch = std::min(context_size, 512);
        params.n_threads = threads;
        params.n_threads_batch = threads;
        context_.reset(llama_init_from_model(model_->handle, params));
        if (!context_) throw std::runtime_error("context initialization failed");
        sampler_.reset(llama_sampler_init_greedy());
        if (!sampler_) throw std::runtime_error("sampler initialization failed");
    }
    bool next(std::string &text) override {
        text.clear();
        if (ended_) return false;
        llama_batch batch = prefilled_ ? llama_batch_get_one(&previous_, 1) :
                                        llama_batch_get_one(prompt_.data(), prompt_.size());
        if (llama_decode(context_.get(), batch) != 0) throw std::runtime_error("decode failed");
        prefilled_ = true;
        previous_ = llama_sampler_sample(sampler_.get(), context_.get(), -1);
        if (llama_vocab_is_eog(vocab_, previous_)) ended_ = true;
        else {
            std::vector<char> piece(128);
            int n = llama_token_to_piece(vocab_, previous_, piece.data(), piece.size(), 0, false);
            if (n < 0) {
                piece.resize(-n);
                n = llama_token_to_piece(vocab_, previous_, piece.data(), piece.size(), 0, false);
            }
            if (n < 0) throw std::runtime_error("token conversion failed");
            pending_.append(piece.data(), n);
            if (++generated_ >= limit_) { ended_ = true; reason_ = "length"; }
        }
        text = utf8_ready(pending_, ended_);
        return !ended_;
    }
    std::string finish_reason() const override { return reason_; }
};
class LlamaEngine : public Engine {
    std::shared_ptr<Model> model_ = std::make_shared<Model>();
    std::string alias_;
    int context_, threads_;
public:
    LlamaEngine(const std::string &file, std::string alias, int context, int threads, int gpu_layers)
        : alias_(std::move(alias)), context_(context), threads_(threads) {
        if (context < 256 || context > 32768 || threads < 1) throw std::invalid_argument("invalid engine limits");
        ggml_backend_load_all();
        auto params = llama_model_default_params();
        params.n_gpu_layers = gpu_layers;
        model_->handle = llama_model_load_from_file(file.c_str(), params);
        if (!model_->handle) throw std::runtime_error("GGUF model load failed");
        if (!llama_model_chat_template(model_->handle, nullptr)) throw std::runtime_error("instruct chat template required");
    }
    Json model_info() const override {
        char description[256]{};
        llama_model_desc(model_->handle, description, sizeof(description));
        return {{"id", alias_}, {"object", "model"}, {"owned_by", "local"},
                {"description", description}, {"context_length", context_}};
    }
    std::shared_ptr<Generation> start_chat(const Chat &chat) override {
        return std::make_shared<LlamaGeneration>(model_, chat, context_, threads_);
    }
};
}
std::unique_ptr<Engine> make_llama_engine(const std::string &file, const std::string &alias,
                                       int context, int threads, int gpu_layers) {
    return std::make_unique<LlamaEngine>(file, alias, context, threads, gpu_layers);
}
}
