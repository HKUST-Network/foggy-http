#include "foggy_http.h"
#include <chrono>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(int argc, char **argv) {
    try {
        if (argc < 3) throw std::runtime_error("usage: foggy-http-server date|echo PORT [--chunked]");
        std::string kind = argv[1];
        bool chunked = argc == 4 && std::string(argv[3]) == "--chunked";
        foggy::Server server;
        if (kind == "date") {
            server.get("/date", [](const foggy::Request &) {
                auto now = std::time(nullptr);
                std::tm tm{};
                localtime_r(&now, &tm);
                char date[32];
                std::strftime(date, sizeof(date), "%Y-%m-%d\n", &tm);
                foggy::Response response;
                response.headers["Content-Type"] = "text/plain; charset=utf-8";
                response.body = date;
                return response;
            });
        } else if (kind == "echo") {
            server.post("/echo", [chunked](const foggy::Request &request) {
                foggy::Response response;
                response.headers["Content-Type"] = "text/plain; charset=utf-8";
                if (!chunked) response.body = request.body;
                else response.next = [text = request.body, at = size_t(0)](std::string &out) mutable {
                    if (at == text.size()) return false;
                    // ASCII demo: one character per 100 ms.
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    out.assign(1, text[at++]);
                    return at < text.size();
                };
                return response;
            });
        } else throw std::runtime_error("unknown service");
        std::cout << "READY " << kind << " " << argv[2] << std::endl;
        server.listen(std::stoi(argv[2]));
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
