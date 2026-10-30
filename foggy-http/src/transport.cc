/* C ABI boundary: only this adapter includes the C++ FoggyTCP header. */
#include "transport.h"
#include "foggy_tcp.h"
#include <errno.h>
#include <stdio.h>

foggy_transport_t foggy_transport_open(const char *host, int port, int listener) {
    if (!host || port < 1 || port > 65535) { errno = EINVAL; return NULL; }
    char service[6];
    snprintf(service, sizeof(service), "%d", port);
    void *socket = foggy_socket(listener ? TCP_LISTENER : TCP_INITIATOR, service, host);
    if (!socket && errno == 0) errno = EIO;
    return socket;
}

void foggy_transport_close(foggy_transport_t *transport) {
    if (*transport) foggy_close(*transport);
    *transport = NULL;
}

int foggy_transport_read(foggy_transport_t transport, void *buffer, size_t size, int timeout_ms) {
    if (!transport || !buffer || !size) { errno = EINVAL; return -1; }
    /* One application reader per socket; dependency source/API are unchanged. */
    struct foggy_socket_t *state = (struct foggy_socket_t *)transport;
    struct timespec deadline = {0, 0};
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
            errno = result;
            return result == ETIMEDOUT ? -2 : -1;
        }
    }
    pthread_mutex_unlock(&state->recv_lock);
    return foggy_read(transport, buffer, (int)(size < 4096 ? size : 4096));
}

int foggy_transport_write(foggy_transport_t transport, const void *data, size_t size) {
    if (!transport || (!data && size)) { errno = EINVAL; return -1; }
    const char *bytes = (const char *)data;
    for (size_t at = 0; at < size;) {
        size_t count = size - at < 4096 ? size - at : 4096;
        if (foggy_write(transport, bytes + at, (int)count) != 0) { errno = EIO; return -1; }
        at += count;
    }
    return 0;
}

int foggy_transport_local_port(foggy_transport_t transport) {
    return transport ? ((struct foggy_socket_t *)transport)->my_port : -1;
}
