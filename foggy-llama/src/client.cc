#include "client_flow.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
int number(const std::string &text, int low, int high, const std::string &name) {
    size_t used = 0;
    int value = std::stoi(text, &used);
    if (used != text.size() || value < low || value > high)
        throw std::invalid_argument("invalid " + name);
    return value;
}

}

int main(int argc, char **argv) {
    try {
        std::string host = "127.0.0.1", command = "models", prompt, alias;
        int port = 8083, max_tokens = 64;
        bool single = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "usage: foggy-client [--host HOST] [--port PORT] health|models|chat\n"
                             "       [--model ALIAS] [--prompt TEXT] [--max-tokens 1..128]\n"
                             "chat without --prompt reads interactive turns; /quit exits.\n";
                return 0;
            }
            if (arg == "health" || arg == "models" || arg == "chat") { command = arg; continue; }
            if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
            std::string value = argv[++i];
            if (arg == "--host") host = value;
            else if (arg == "--port") port = number(value, 1, 65535, "port");
            else if (arg == "--model") alias = value;
            else if (arg == "--prompt") { prompt = value; single = true; }
            else if (arg == "--max-tokens") max_tokens = number(value, 1, 128, "max-tokens");
            else throw std::invalid_argument("unknown option " + arg);
        }
        if (host.empty()) throw std::invalid_argument("host cannot be empty");
        foggy_http_client_t client{};
        if (foggy_http_client_init(&client, host.c_str(), port, 1) < 0)
            throw std::runtime_error(std::string("client init failed: ") + std::strerror(errno));
        const std::unique_ptr<foggy_http_client_t, decltype(&foggy_http_client_free)>
            cleanup(&client, foggy_http_client_free);
        if (command == "health") {
            std::cout << foggy_llama::health_status(&client).dump(2) << std::endl;
            return 0;
        }
        const auto models = foggy_llama::discover_models(&client);
        if (command == "models") { std::cout << models.dump(2) << std::endl; return 0; }
        alias = foggy_llama::discover_model(models, alias);
        std::cerr << "MODEL " << alias << std::endl;
        auto history = foggy_llama::Json::array();
        for (;;) {
            if (!single) {
                std::cerr << "user> " << std::flush;
                if (!std::getline(std::cin, prompt) || prompt == "/quit") break;
                if (prompt.empty()) continue;
            }
            auto turn = foggy_llama::chat_turn(&client, alias, history, prompt, max_tokens,
                [](const std::string &text) { std::cout << text << std::flush; });
            std::cout << std::endl;
            std::cerr << "DONE messages=" << turn.messages << " first_ms=" << turn.first_ms
                      << " total_ms=" << turn.total_ms << std::endl;
            if (single) break;
        }
    } catch (const std::exception &error) {
        std::cerr << "ERROR " << error.what() << std::endl;
        return 1;
    }
}
