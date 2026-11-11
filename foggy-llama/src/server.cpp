#include "api.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
int number(const std::string &text, int low, int high, const std::string &name) {
    size_t used = 0;
    int value = std::stoi(text, &used);
    if (used != text.size() || value < low || value > high)
        throw std::invalid_argument("invalid " + name);
    return value;
}

class Server {
public:
    foggy_http_server_t value{};
    Server() { foggy_http_server_init(&value); }
    ~Server() { foggy_http_server_free(&value); }
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
};
}

int main(int argc, char **argv) {
    try {
        std::string file, alias = "course-model";
        int port = 8083, delay = 100, sessions = 0;
        int context = 2048, threads = 2, gpu = 0;
        bool mock = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "usage: foggy-server --mock | --model local.gguf\n"
                             "       [--alias course-model] [--port 8083] [--ctx 2048]\n"
                             "       [--threads 2] [--gpu-layers 0] [--mock-delay-ms 100]\n"
                             "       [--sessions COUNT] (0 serves until stopped)\n";
                return 0;
            }
            if (arg == "--mock") { mock = true; continue; }
            if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
            std::string value = argv[++i];
            if (arg == "--model") file = value;
            else if (arg == "--alias") alias = value;
            else if (arg == "--port") port = number(value, 1, 65535, "port");
            else if (arg == "--ctx") context = number(value, 256, 32768, "ctx");
            else if (arg == "--threads") threads = number(value, 1, 1024, "threads");
            else if (arg == "--gpu-layers") gpu = number(value, 0, 1024, "gpu-layers");
            else if (arg == "--mock-delay-ms") delay = number(value, 0, 60000, "mock-delay-ms");
            else if (arg == "--sessions") sessions = number(value, 0, 1000000, "sessions");
            else throw std::invalid_argument("unknown option " + arg);
        }
        if (alias.empty()) throw std::invalid_argument("model alias cannot be empty");
        if (mock && !file.empty()) throw std::invalid_argument("choose --mock or --model");
        std::unique_ptr<foggy_llama::Engine> engine;
        if (mock) engine = foggy_llama::make_mock_engine(alias, delay);
        else {
            if (file.empty()) throw std::invalid_argument("use --model local.gguf or --mock");
#ifdef FOGGY_WITH_LLAMA
            engine = foggy_llama::make_llama_engine(file, alias, context, threads, gpu);
#else
            (void)context;
            (void)threads;
            (void)gpu;
            throw std::invalid_argument("built without llama.cpp; use --mock or rebuild with FOGGY_WITH_LLAMA=ON");
#endif
        }
        foggy_llama::ServiceContext service{*engine};
        Server server;
        if (foggy_http_server_get(&server.value, "/health", foggy_llama::health, &service) < 0 ||
            foggy_http_server_get(&server.value, "/v1/models", foggy_llama::models, &service) < 0 ||
            foggy_http_server_post(&server.value, "/v1/chat/completions", foggy_llama::chat, &service) < 0)
            throw std::runtime_error(std::string("route registration failed: ") + std::strerror(errno));
        std::cout << "MODEL " << engine->model_info().dump() << std::endl;
        // The sequential course listener opens its socket after this marker.
        std::cout << "READY port=" << port << " mock=" << mock << std::endl;
        if (foggy_http_server_listen(&server.value, port, sessions) < 0)
            throw std::runtime_error(std::string("server failed: ") + std::strerror(errno));
    } catch (const std::exception &error) {
        std::cerr << "ERROR " << error.what() << std::endl;
        return 1;
    }
}
