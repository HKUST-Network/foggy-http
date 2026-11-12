#include "engine.h"
#include "llama.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace foggy_llama {
namespace {
struct LlamaEngine {
    std::shared_ptr<llama_model> model;
    std::string alias;
    int context_size, threads;
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
struct LlamaGeneration {
    std::shared_ptr<llama_model> model;
    std::unique_ptr<llama_context, decltype(&llama_free)> context{nullptr, llama_free};
    std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler{nullptr, llama_sampler_free};
    std::vector<llama_token> prompt;
    const llama_vocab *vocab;
    bool prefilled = false, ended = false;
    int limit, generated = 0;
    llama_token previous = 0;
    const char *reason = "stop";
    std::string pending;
};

bool llama_next(void *context, std::string &text, const char **finish_reason) {
    auto &state = *static_cast<LlamaGeneration *>(context);
    text.clear();
    if (state.ended) {
        *finish_reason = state.reason;
        return false;
    }
    llama_batch batch = state.prefilled ? llama_batch_get_one(&state.previous, 1) :
                                         llama_batch_get_one(state.prompt.data(), state.prompt.size());
    if (llama_decode(state.context.get(), batch) != 0) throw std::runtime_error("decode failed");
    state.prefilled = true;
    state.previous = llama_sampler_sample(state.sampler.get(), state.context.get(), -1);
    if (llama_vocab_is_eog(state.vocab, state.previous)) state.ended = true;
    else {
        std::vector<char> piece(128);
        int n = llama_token_to_piece(state.vocab, state.previous, piece.data(), piece.size(), 0, false);
        if (n < 0) {
            piece.resize(-n);
            n = llama_token_to_piece(state.vocab, state.previous, piece.data(), piece.size(), 0, false);
        }
        if (n < 0) throw std::runtime_error("token conversion failed");
        state.pending.append(piece.data(), n);
        if (++state.generated >= state.limit) {
            state.ended = true;
            state.reason = "length";
        }
    }
    text = utf8_ready(state.pending, state.ended);
    *finish_reason = state.reason;
    return !state.ended;
}

Json llama_model_info(const void *context) {
    const auto &state = *static_cast<const LlamaEngine *>(context);
    char description[256]{};
    llama_model_desc(state.model.get(), description, sizeof(description));
    return {{"id", state.alias}, {"object", "model"}, {"owned_by", "local"},
            {"description", description}, {"context_length", state.context_size}};
}

Generation llama_start_chat(void *context, const Chat &chat) {
    const auto &engine = *static_cast<const LlamaEngine *>(context);
    auto state = std::make_shared<LlamaGeneration>();
    state->model = engine.model;
    state->vocab = llama_model_get_vocab(state->model.get());
    state->limit = chat.max_tokens;

    std::vector<llama_chat_message> messages;
    for (const auto &m : chat.messages) messages.push_back({m.role.c_str(), m.content.c_str()});
    auto tmpl = llama_model_chat_template(state->model.get(), nullptr);
    if (!tmpl) throw std::invalid_argument("model has no chat template");
    int size = llama_chat_apply_template(tmpl, messages.data(), messages.size(), true, nullptr, 0);
    if (size < 0 || size > 256*1024) throw std::invalid_argument("unsupported or oversized chat template");
    std::vector<char> prompt(size);
    int written = llama_chat_apply_template(tmpl, messages.data(), messages.size(), true, prompt.data(), prompt.size());
    if (written != size) throw std::runtime_error("chat template changed size");
    int count = llama_tokenize(state->vocab, prompt.data(), prompt.size(), nullptr, 0, true, true);
    if (count >= 0) throw std::invalid_argument("empty prompt");
    count = -count;
    if (count + state->limit > engine.context_size)
        throw std::invalid_argument("prompt plus max_tokens exceeds context");
    state->prompt.resize(count);
    if (llama_tokenize(state->vocab, prompt.data(), prompt.size(), state->prompt.data(), count, true, true) != count)
        throw std::runtime_error("tokenize failed");

    auto params = llama_context_default_params();
    params.n_ctx = engine.context_size;
    params.n_batch = engine.context_size;
    params.n_ubatch = std::min(engine.context_size, 512);
    params.n_threads = engine.threads;
    params.n_threads_batch = engine.threads;
    state->context.reset(llama_init_from_model(state->model.get(), params));
    if (!state->context) throw std::runtime_error("context initialization failed");
    state->sampler.reset(llama_sampler_init_greedy());
    if (!state->sampler) throw std::runtime_error("sampler initialization failed");
    return {state, llama_next};
}
}

Engine make_llama_engine(const std::string &file, const std::string &alias,
                         int context, int threads, int gpu_layers) {
    if (context < 256 || context > 32768 || threads < 1) throw std::invalid_argument("invalid engine limits");
#if defined(__APPLE__)
    // The release includes Metal. Skip device initialization for CPU-only runs.
    if (gpu_layers == 0) setenv("GGML_METAL_DEVICES", "0", 0);
#endif
    ggml_backend_load_all();
    auto params = llama_model_default_params();
    params.n_gpu_layers = gpu_layers;
    auto *handle = llama_model_load_from_file(file.c_str(), params);
    if (!handle) throw std::runtime_error("GGUF model load failed");
    std::shared_ptr<llama_model> model(handle, llama_model_free);
    if (!llama_model_chat_template(model.get(), nullptr)) throw std::runtime_error("instruct chat template required");
    auto state = std::make_shared<LlamaEngine>(LlamaEngine{model, alias, context, threads});
    return {state, llama_model_info, llama_start_chat};
}
}
