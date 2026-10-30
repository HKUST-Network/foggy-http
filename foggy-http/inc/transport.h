#ifndef FOGGY_TRANSPORT_H
#define FOGGY_TRANSPORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque FoggyTCP v2 handle. The dependency remains compiled as C++. */
typedef void *foggy_transport_t;
foggy_transport_t foggy_transport_open(const char *host, int port, int listener);
void foggy_transport_close(foggy_transport_t *transport);
/* >0 = bytes; 0 = peer EOF; -2 = timeout; -1 = error. */
int foggy_transport_read(foggy_transport_t transport, void *buffer, size_t size, int timeout_ms);
/* Queue all bytes in bounded batches; 0 = success, -1 = error. */
int foggy_transport_write(foggy_transport_t transport, const void *data, size_t size);
int foggy_transport_local_port(foggy_transport_t transport);

#ifdef __cplusplus
}
#endif
#endif
