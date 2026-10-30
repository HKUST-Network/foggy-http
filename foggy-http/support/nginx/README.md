# Fixed FoggyHTTP NGINX module

This C dynamic module supplies the read-only Stream variable `$foggy_http_path`.
It inspects the first FoggyTCP UDP datagram in NGINX's existing receive buffer,
checks framing, and copies the HTTP request path into session-owned memory.
It does not consume or change the packet. Standard `map` and `proxy_pass`
directives select and forward to the backend. No Lua runtime is involved.

The initial sequence-zero datagram must contain the complete HTTP request line,
including CRLF. Subsequent datagrams, including ACK and FIN, follow the initial
session's upstream. A session does not switch between application backends.

Install ordinary `nginx` and `libnginx-mod-stream`, GCC, Make, and development
headers. On systems providing `nginx-dev`, its version must match NGINX and the
script uses `/usr/share/nginx/src`. Ubuntu 22.04 has no nginx-dev package: setup
uses the bundled SHA-256-verified upstream NGINX 1.18.0 source headers and installs
PCRE/zlib/SSL development dependencies instead. The running NGINX remains the
APT-provided binary. Run `sudo bash install.sh`; it executes only
`make -f objs/Makefile modules` and installs
`/usr/lib/nginx/modules/ngx_stream_foggy_http_module.so`. It validates loading
with `nginx -t` and verifies the NGINX executable was not changed.

`lab/setup.sh` in the student project and `setup.sh` in the autograder automate
these steps. Students use the provided module without editing its source.
An instructor may provision the matching binary in the course VM instead.
The module must match the NGINX runtime and CPU architecture; rerun setup after
upgrading NGINX. Only module sources are distributed, so each environment builds
its own compatible binary.
