#include "foggy_http.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(int argc, char **argv) {
    try {
        if (argc < 4) throw std::runtime_error("usage: foggy-http-client HOST PORT date|echo [TEXT] [--chunked] [--no-keep-alive] [--repeat N] [--interval S]");
        std::string task = argv[3], text = "Foggy HTTP streaming";
        bool chunks = false, keep = true;
        int repeat = 1, interval = 0;
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--chunked") chunks = true;
            else if (arg == "--no-keep-alive") keep = false;
            else if (arg == "--repeat" && i + 1 < argc) repeat = std::stoi(argv[++i]);
            else if (arg == "--interval" && i + 1 < argc) interval = std::stoi(argv[++i]);
            else text = arg;
        }
        foggy::Client client(argv[1], std::stoi(argv[2]), keep);
        for (int i = 0; i < repeat; ++i) {
            if (i) std::this_thread::sleep_for(std::chrono::seconds(interval));
            auto start = std::chrono::steady_clock::now();
            foggy::Receiver receive;
            if (chunks) receive = [&](const std::string &part) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                std::cout << part << std::flush;
                std::cerr << "CHUNK " << ms << " " << part.size() << std::endl;
            };
            auto response = task == "date" ? client.get("/date") : client.post("/echo", text, receive);
            if (!chunks || task == "date") std::cout << response.body << std::flush;
            std::cerr << "RESPONSE " << response.status << " local_port=" << client.connection_port() << std::endl;
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "ERROR " << error.what() << std::endl;
        return 1;
    }
}
