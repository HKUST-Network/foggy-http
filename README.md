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
compiled using `-std=c11 -x c`. FoggyTCP and its C ABI adapter use C++17. Build
with GCC/Clang, a C++17 compiler, Make, and Python 3.9+:

```sh
make -j2
```

## Applications and direct access

| Executable | Operation | Default port | Request path |
| --- | --- | ---: | --- |
| `data-client` | GET the current date | Destination UDP 80 | `/checkpoint-3/date` |
| `data-server` | Return `YYYY-MM-DD\n` | Listen on UDP 8081 | `/checkpoint-3/date` |
| `echo-client` | POST text and print the returned body | Destination UDP 80 | `/checkpoint-3/echo` |
| `echo-server` | Return the exact request body | Listen on UDP 8082 | `/checkpoint-3/echo` |

Run the two servers in separate server terminals:

```sh
TZ=Asia/Hong_Kong ./build/data-server
./build/echo-server
```

Run direct backend checks from the client VM (replace the example server IP):

```sh
./build/data-client 10.0.1.1 --port 8081 --no-keep-alive
./build/echo-client 10.0.1.1 hello --port 8082 --no-keep-alive
```

This repository is an **unfinished raw starter**. Initially GET sends only a
raw path, POST sends only raw body bytes, and the server returns raw data before
closing. These small direct exchanges check connectivity. Implement HTTP before
attempting the path-based NGINX tests.

## Student work

Modify exactly two files:

- `foggy-http/src/foggy_http.cc`: HTTP serialization/parsing, message framing,
  connection management, chunked encoding, and incremental decoding.
- `foggy-http/conf/nginx-stream.conf`: standard NGINX path-to-backend rules in
  the `map` block.

Keep the public C API, four applications, transport, NGINX module, and build
files unchanged. The API is in `foggy-http/inc/foggy_http.h`.

Required HTTP behavior:

1. GET `/checkpoint-3/date` and POST `/checkpoint-3/echo`; include Host and
   Connection, and Content-Length for POST, including an empty body.
2. Parse complete requests before invoking the method/path handler. Preserve
   exact body bytes, including NUL and UTF-8; lengths count bytes.
3. Buffer split messages and retain bytes read beyond the current message.
   Bound each start/header line to 8 KiB, the whole header section to 16 KiB,
   and ordinary or cumulative chunked payloads to 1 MiB. Reject invalid framing.
4. Reuse HTTP/1.1 connections and honor Connection: close. HTTP/1.0 closes by
   default. Determine response completion from framing instead of waiting for EOF.
5. Close server connections after 3 seconds of idle time following a response.
   A client waiting 10 seconds between GETs must establish a new usable connection.
6. Enforce a 5-second incomplete-message read deadline; streaming clients may
   restart the deadline per chunk.

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

On the Debian/Ubuntu **server VM**, install the environment once from the
project root:

```sh
sudo bash lab/setup.sh
```

The script installs ordinary NGINX, its Stream module, and the C/C++ build tools.
Where available, it uses matching nginx-dev headers. Ubuntu 22.04 has no
nginx-dev package, so the project bundles verified NGINX 1.18.0 source headers
for that runtime; setup installs the corresponding build dependencies and checks
the archive's SHA-256. In either case it builds only the supplied dynamic module
and installs it at:

```text
/usr/lib/nginx/modules/ngx_stream_foggy_http_module.so
```

The NGINX executable is not rebuilt or replaced. Module loading is checked with
`nginx -t`. Setup records the NGINX/package version, architecture, and source
identity; rerun it after upgrading NGINX or changing the fixed support code.
Students do not write the C module. Instructors may provision a matching module
in the VM in advance. A compiled module must match the NGINX runtime and CPU
architecture, so binaries are not included in source ZIPs.

Complete the `map` rules in `foggy-http/conf/nginx-stream.conf`. Use case-sensitive regular expression keys to distinguish the two exact HTTP
paths; see the [standard map reference](https://nginx.org/en/docs/stream/ngx_stream_map_module.html). Keep the
proxy timeout at 30 seconds, above the HTTP idle timeout, and leave the number
of expected UDP response datagrams unlimited for ACKs, FINs, and streamed data.

Keep both backend servers running. In another server terminal:

```sh
python3 lab/nginx.py --root "$PWD/runtime/nginx" \
    --student-config "$PWD/foggy-http/conf/nginx-stream.conf"
sudo /usr/sbin/nginx -t \
    -p "$PWD/runtime/nginx/" -c "$PWD/runtime/nginx/nginx.conf"
sudo /usr/sbin/nginx \
    -p "$PWD/runtime/nginx/" -c "$PWD/runtime/nginx/nginx.conf" -g 'daemon off;'
```

The generator loads the standard Stream module and the fixed C module in an
isolated configuration. Logs and the PID stay under runtime/nginx/. Existing
system configuration does not need editing. Root is used here to bind UDP 80;
the two backend applications run as an ordinary user. Stop the foreground proxy
with Ctrl+C before starting another instance on UDP 80.

Clients use port 80 by default:

```sh
./build/data-client 10.0.1.1
./build/echo-client 10.0.1.1 0123456789abcdefghij
./build/data-client 10.0.1.1 --repeat 3
./build/data-client 10.0.1.1 --repeat 2 --interval 10
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

## Wireshark

Load `foggy-http/utils/foggy-tcp.lua` before
`foggy-http/utils/foggy-http.lua`. These are passive Wireshark dissectors. Capture
`udp port 80 or udp port 8081 or udp port 8082`; use the server's `lo` or `any`
interface to include local proxy-to-backend traffic. Start with a fresh capture
that includes the session beginning.
