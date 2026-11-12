#include "transport.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "usage: foggy-http-probe PORT METHOD PATH\n";
        return 2;
    }
    try {
        const int port = std::stoi(argv[1]);
        const std::string request = std::string(argv[2]) + " " + argv[3] +
            " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        auto close_connection = [](void *handle) { foggy_transport_close(&handle); };
        const std::unique_ptr<void, decltype(close_connection)>
            connection(foggy_transport_open("127.0.0.1", port, 0), close_connection);
        if (!connection) throw std::runtime_error("could not open a FoggyTCP connection");
        if (foggy_transport_write(connection.get(), request.data(), request.size()) < 0)
            throw std::runtime_error("could not send the request");

        char buffer[2048];
        for (;;) {
            const int count = foggy_transport_read(connection.get(), buffer, sizeof(buffer), 3000);
            if (count < 0) throw std::runtime_error("could not read the HTTP response");
            if (count == 0) return 0;
            std::cout.write(buffer, count);
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
