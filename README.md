# Foggy HTTP — ELEC 3120 CP3

Implement HTTP/1.0 and HTTP/1.1 over the supplied FoggyTCP v2 byte stream, then
configure ordinary NGINX Stream to route both applications through **UDP 80**.
A fixed C dynamic module extracts the HTTP path. Students use standard NGINX
`map` and `proxy_pass` directives; the module source is provided unchanged.

## Get the starter

```sh
git clone --recurse-submodules https://github.com/HKUST-Network/foggy-http.git
cd foggy-http
git submodule update --init --recursive
```

Use the recorded **FoggyTCP v2** submodule, not your CP2 implementation. Source
ZIPs include the dependency. The project root contains Makefile, submit.py,
foggy-http/, thirdparty/, and lab/.

The four applications and HTTP library are C11 code with `.cc` filenames,
compiled using `-std=c11 -x c`. FoggyTCP and its C ABI adapter use C++17.
In **each Debian/Ubuntu VM**, install the environment once, then build from the
project root:

```sh
sudo bash lab/setup.sh
make -j2
```

Setup installs NGINX, its standard Stream module, the compilers, and development
dependencies. Run `make -j2` as an ordinary user after editing your code. It
builds the four applications and the fixed C module at
`build/ngx_stream_foggy_http_module.so`.

## Applications and direct access

| Executable | Operation | Default port | Request path |
| --- | --- | ---: | --- |
| `date-client` | GET the current date | Destination UDP 80 | `/checkpoint-3/date` |
| `date-server` | Return `YYYY-MM-DD\n` | Listen on UDP 8081 | `/checkpoint-3/date` |
| `echo-client` | POST text and print the returned body | Destination UDP 80 | `/checkpoint-3/echo` |
| `echo-server` | Return the exact request body | Listen on UDP 8082 | `/checkpoint-3/echo` |

Run the two servers in separate server terminals. `date-server` sets its process
timezone to `Asia/Hong_Kong` automatically, regardless of the inherited timezone:

```sh
./build/date-server
./build/echo-server
```

Run direct backend checks from the client VM (replace the example server IP):

```sh
./build/date-client 10.0.1.1 --port 8081 --no-keep-alive
./build/echo-client 10.0.1.1 hello --port 8082 --no-keep-alive
```

This repository is an **unfinished raw starter**. Initially GET sends only a
raw path, POST sends only raw body bytes, and the server returns raw data before
closing the connection after every response. The starter does not support
HTTP messages, keep-alive, or incremental streaming yet.

For the initial tests, connect directly to the backend using `--port 8081` for
the date service or `--port 8082` for echo. Both clients default to UDP 80,
the NGINX entry point. Implement HTTP and complete the routing configuration
before testing through that port. A successful raw exchange checks connectivity.

## Student work

Modify exactly two files:

- `foggy-http/src/foggy_http.cc`: HTTP serialization/parsing, message framing,
  connection management, chunked encoding, and incremental decoding.
- `foggy-http/conf/nginx-stream.conf`: fill the two `TODO` path-to-backend rules
  in the complete NGINX configuration template.

Keep the public C API, four applications, transport, NGINX module, and build
files unchanged. The API is in `foggy-http/inc/foggy_http.h`.

The main implementation points in `foggy_http.cc` are `request()` on the client
and `foggy_http_server_listen()` on the server. Their `TODO` comments identify
the missing HTTP framing, connection reuse, and streaming behavior.

Required HTTP behavior:

1. GET `/checkpoint-3/date` and POST `/checkpoint-3/echo`; include Host and
   Connection, and Content-Length for POST, including an empty body.
2. Parse complete requests before invoking the method/path handler. Preserve
   exact body bytes, including NUL and UTF-8; lengths count bytes.
3. Buffer split messages and retain bytes read beyond the current message.
   Validate message framing before processing the body.
4. Reuse HTTP/1.1 connections and honor Connection: close. HTTP/1.0 closes by
   default. Determine response completion from framing instead of waiting for EOF.
5. Close server connections after 3 seconds of idle time following a response.
   A client waiting 10 seconds between GETs must establish a new usable connection.

## Keep-alive and idle timeout

The supplied clients already parse the keep-alive option and pass it to
`foggy_http_client_init()`. Implement the following behavior in
`foggy-http/src/foggy_http.cc`:

| Client option | HTTP version | Request header |
| --- | --- | --- |
| Default | HTTP/1.1 | `Connection: keep-alive` |
| `--no-keep-alive` | HTTP/1.0 | `Connection: close` |

`--repeat` controls the number of requests, not the HTTP version. The raw
starter stores the keep-alive setting but does not yet implement this behavior.

Keep a persistent connection open after sending a complete response and wait
for the next request. If no new request starts within **3 seconds**, close the
idle connection. Start a new idle interval after each completed response;
sending an active streaming response does not count as idle time. Honor an
explicit `Connection: close` by closing after the response instead.

With `date-server` running, test direct backend access from the client VM:

```sh
./build/date-client 10.0.1.1 --port 8081 --repeat 3
./build/date-client 10.0.1.1 --port 8081 --repeat 2 --interval 10
```

The first command should report the same `local_port` for all three responses.
The second should report a new port for its second response. The assignment
fixes the server's idle timeout at 3 seconds; `--interval 10` only sets the
client's delay between requests. The autograder checks actual connection reuse,
idle closure, and reconnection.

## Chunked echo

Stop the ordinary echo server and restart it with the server-side option:

```sh
./build/echo-server --chunked
```

The fixed producer generates one payload byte every 100 ms. Send HTTP/1.1
`Transfer-Encoding: chunked`, omit Content-Length, encode each piece promptly,
and terminate with `0\r\n\r\n`. A producer return value of 0 may still include
its last nonempty piece; send it before the terminator.

The echo client automatically selects decoding from response headers and
immediately prints received payloads. It has no client-side chunked option:

```sh
./build/echo-client 10.0.1.1 0123456789abcdefghij --port 8082
```

stdout contains only decoded body bytes. stderr reports `CHUNK <ms> <bytes>` and
`RESPONSE <status> local_port=<port>` for timing and connection analysis.

## Ordinary NGINX Stream + fixed C module

NGINX can proxy HTTP at the application layer through its HTTP module, or
forward TCP/UDP traffic at the transport layer through Stream. Here, foggy-http
uses FoggyTCP inside UDP datagrams, which the HTTP module cannot directly
interpret. We therefore use Stream to forward the datagrams and a supplied C
module to parse the initial FoggyTCP packet's HTTP request path.

The proxy path is:

```text
client -> FoggyTCP/UDP :80 -> ordinary NGINX Stream
                             /checkpoint-3/date -> UDP :8081
                             /checkpoint-3/echo -> UDP :8082
```

`checkpoint-3` is part of the HTTP path. The proxy listens on UDP 80. The C module
inspects the initial packet without consuming or modifying it and provides the
read-only `$foggy_http_path` variable. Your standard `map` sets `$backend`, and
`proxy_pass $backend` forwards the session. Actual forwarding remains at the UDP
transport layer; the HTTP path is used only to choose the backend.

After the initial environment setup, `make -j2` builds the supplied module at:

```text
build/ngx_stream_foggy_http_module.so
```

The build uses matching NGINX development headers, with bundled,
SHA-256-verified NGINX 1.18.0 sources for Ubuntu 22.04. It builds only the course
module and leaves the NGINX executable unchanged. Run `make` again after
upgrading NGINX: a module must match its runtime and CPU architecture. Use the
provided C source unchanged.

`foggy-http/conf/nginx-stream.conf` is a **complete configuration template**.
It already loads the standard Stream and course modules and contains the
`events`, `stream`, and `server` blocks. Fill only the two `TODO` mappings. Use
case-sensitive regular expression keys for the exact HTTP paths; see the
[standard map reference](https://nginx.org/en/docs/stream/ngx_stream_map_module.html).
Keep the supplied structure and the 30-second proxy timeout, which is longer
than the HTTP idle timeout. Leave the expected UDP response count unlimited for
ACKs, FINs, and streamed data.

Keep both backend servers running. In another server terminal:

```sh
sudo /usr/sbin/nginx -t -p "$PWD/" \
    -c "$PWD/foggy-http/conf/nginx-stream.conf" \
    -g 'pid build/nginx.pid; error_log stderr info;'
sudo /usr/sbin/nginx -p "$PWD/" \
    -c "$PWD/foggy-http/conf/nginx-stream.conf" \
    -g 'pid build/nginx.pid; error_log stderr info; daemon off;'
```

These commands validate and run your configuration directly. The prefix `-p`
resolves the module's `build/` path from the project root. The PID is stored at
`build/nginx.pid`, and `FOGGY_PATH` selections and other diagnostics appear in
the terminal. Root is used here to bind UDP 80; the two backend applications
run as an ordinary user. Stop the foreground proxy with Ctrl+C before starting
another instance on UDP 80.

Clients use port 80 by default:

```sh
./build/date-client 10.0.1.1
./build/echo-client 10.0.1.1 0123456789abcdefghij
./build/date-client 10.0.1.1 --repeat 3
./build/date-client 10.0.1.1 --repeat 2 --interval 10
```

Lab routing scope:

- The initial sequence-zero FoggyTCP datagram contains the entire HTTP request
  line, including CRLF. Write the complete line/header section in one operation.
- Remaining headers and bodies may span datagrams and reads.
- Each UDP session stays with the backend selected by its first request.
  Date and echo use separate connections.

## Build and submit

```sh
make -j2
python3 submit.py
python3 submit.py --verify submit.zip
```

Upload the generated flat `submit.zip`, containing exactly:

```text
foggy_http.cc
nginx-stream.conf
SHA256SUMS
```

SHA256SUMS is generated automatically. The submitter checks current files against
the recorded build; rebuild after editing either file. The fixed applications,
transport, module sources and binaries are provided by the grader.

Submit the complete `nginx-stream.conf` with your mapping rules filled in. The
autograder adjusts the `.so` loading paths to its own modules and tests your
configuration. Do not include compiled `.so` files in the submission.

## Wireshark

Load `foggy-http/utils/foggy-tcp.lua` before
`foggy-http/utils/foggy-http.lua`. These are passive Wireshark dissectors. Capture
`udp port 80 or udp port 8081 or udp port 8082`; use the server's `lo` or `any`
interface to include local proxy-to-backend traffic. Start with a fresh capture
that includes the session beginning.

Only correctly formatted HTTP messages can be decoded as HTTP by the supplied
dissectors. The raw starter can appear as FoggyTCP traffic without decoded HTTP
fields. After implementing HTTP, inspect start lines, headers, CRLF delimiters,
and body framing with the `http` display filter.

A chunked response may be shown as one complete message when reassembly
finishes. Use UDP packet timestamps and the echo client's `CHUNK` records to
check that data arrived incrementally.
