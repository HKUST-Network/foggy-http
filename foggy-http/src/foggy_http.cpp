#include "foggy_http.h"
#include <stdexcept>

// Deliberately incomplete starter. These functions send raw application bytes,
// NOT valid HTTP/1.0. "HTTP/1.0-like" here means one exchange per connection only.
namespace foggy {
Client::Client(std::string host, int port, bool keep)
    : host_(std::move(host)), port_(port), keep_alive_(keep) {}
void Client::close() { connection_.reset(); input_.clear(); }
int Client::connection_port() const { return connection_ ? connection_->local_port() : -1; }
Response Client::get(const std::string &path) { return request("GET", path, "", {}); }
Response Client::post(const std::string &path, const std::string &body, Receiver cb) {
    return request("POST", path, body, std::move(cb));
}
Response Client::request(const std::string &method, const std::string &path,
                         const std::string &body, Receiver receive) {
    // TODO: serialize start-line, headers, Content-Length, Host and Connection.
    // TODO: reuse a live connection; parse responses and decode chunked incrementally.
    close();
    connection_ = std::make_unique<Transport>(host_, port_);
    connection_->write(method == "GET" ? path : body);
    Response response;
    char data[4096];
    for (;;) {
        int n = connection_->read(data, sizeof(data), 5000);
        if (n < 0) throw std::runtime_error("raw read failed");
        if (n == 0) break;
        response.body.append(data, n);
    }
    if (receive) receive(response.body); // One buffered delivery, not streaming.
    close();
    return response;
}
void Server::get(const std::string &path, Handler h) { routes_[{"GET", path}] = std::move(h); }
void Server::post(const std::string &path, Handler h) { routes_[{"POST", path}] = std::move(h); }
void Server::listen(int port, int session_limit) {
    for (int session = 0; session_limit == 0 || session < session_limit; ++session) {
        Transport io("0.0.0.0", port, true);
        char data[4096];
        int n = io.read(data, sizeof(data), 30000);
        if (n <= 0 || routes_.empty()) continue;
        // TODO: maintain a per-connection buffer and parse full HTTP requests.
        // Raw starter supports a small payload delivered by one read only.
        Request request;
        request.body.assign(data, n);
        Response response = routes_.begin()->second(request);
        if (response.next) {
            bool more;
            do {
                std::string part;
                more = response.next(part);
                response.body += part;
            } while (more);
        }
        // TODO: serialize response; chunk producer output; loop for keep-alive.
        io.write(response.body);
    }
}
}
