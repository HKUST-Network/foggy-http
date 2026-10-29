#pragma once
#include "transport.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace foggy {
using Headers = std::map<std::string, std::string>;
using Receiver = std::function<void(const std::string &)>;
struct Request {
    std::string method, target, body;
    std::string version = "HTTP/1.1";
    Headers headers;
};
struct Response {
    int status = 200;
    std::string body;
    Headers headers;
    // Return false after the last output (which may still be nonempty).
    std::function<bool(std::string &)> next;
};
using Handler = std::function<Response(const Request &)>;
class Client {
public:
    Client(std::string host, int port, bool keep_alive = true);
    Response get(const std::string &path);
    Response post(const std::string &path, const std::string &body, Receiver on_data = {});
    void close();
    int connection_port() const;
private:
    Response request(const std::string &method, const std::string &path,
                     const std::string &body, Receiver on_data);
    std::string host_, input_;
    int port_;
    bool keep_alive_;
    std::unique_ptr<Transport> connection_;
    std::chrono::steady_clock::time_point last_used_{};
};
class Server {
public:
    void get(const std::string &path, Handler handler);
    void post(const std::string &path, Handler handler);
    // Sequential connections only. session_limit=0 runs until process termination.
    void listen(int port, int session_limit = 0);
private:
    std::map<std::pair<std::string, std::string>, Handler> routes_;
};
}
