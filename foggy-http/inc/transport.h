#pragma once
#include <cstddef>
#include <string>

namespace foggy {
// HTTP-side timed-wait adapter to the FoggyTCP v2 submodule.
// >0 = bytes; 0 = peer EOF; -2 = local timeout; -1 = error.
class Transport {
public:
    Transport(const std::string &host, int port, bool listener = false);
    ~Transport();
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;
    int read(char *buffer, size_t size, int timeout_ms);
    void write(const std::string &bytes);
    int local_port() const;
private:
    void *socket_;
};
}
