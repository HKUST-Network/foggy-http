#include "transport.h"
#include "foggy_tcp.h"
#include <algorithm>
#include <cerrno>
#include <stdexcept>

namespace foggy {
Transport::Transport(const std::string &host, int port, bool listener) {
    if (port < 1 || port > 65535) throw std::runtime_error("invalid port");
    socket_ = foggy_socket(listener ? TCP_LISTENER : TCP_INITIATOR,
                          std::to_string(port).c_str(), host.c_str());
    if (!socket_) throw std::runtime_error("FoggyTCP open failed");
}
Transport::~Transport() { foggy_close(socket_); }
int Transport::read(char *buffer, size_t size, int timeout_ms) {
    if (!size) return -1;
    // FoggyTCP v2 exposes these synchronization fields.
    // One application reader per socket; transport source and API are unchanged.
    auto *state = static_cast<foggy_socket_t *>(socket_);
    timespec deadline{};
    if (timeout_ms >= 0) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeout_ms / 1000;
        deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&state->recv_lock);
    while (state->received_len == 0 && !state->peer_closed) {
        int result = timeout_ms < 0
            ? pthread_cond_wait(&state->wait_cond, &state->recv_lock)
            : pthread_cond_timedwait(&state->wait_cond, &state->recv_lock, &deadline);
        if (result != 0 && state->received_len == 0 && !state->peer_closed) {
            pthread_mutex_unlock(&state->recv_lock);
            return result == ETIMEDOUT ? -2 : -1;
        }
    }
    pthread_mutex_unlock(&state->recv_lock);
    return foggy_read(socket_, buffer, std::min(size, size_t(4096)));
}
void Transport::write(const std::string &bytes) {
    for (size_t at = 0; at < bytes.size(); at += 4096) {
        auto count = std::min(size_t(4096), bytes.size() - at);
        if (foggy_write(socket_, bytes.data() + at, count) != 0)
            throw std::runtime_error("FoggyTCP write failed");
    }
}
int Transport::local_port() const {
    // Existing upstream field, for diagnostics only; no transfer logic here.
    return static_cast<foggy_socket_t *>(socket_)->my_port;
}
}
