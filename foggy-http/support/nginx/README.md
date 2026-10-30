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
APT-provided binary. From the student project root, install dependencies once
with `sudo bash lab/setup.sh`, then run `make -j2` as your ordinary user. Make
builds the applications and calls the supplied module build helper, which runs
only `make -f objs/Makefile modules` and writes
`build/ngx_stream_foggy_http_module.so`. It validates loading with `nginx -t`
and verifies the NGINX executable was not changed. No configuration generator
is needed: the student configuration is a complete NGINX configuration.

Use `make nginx-module` to build or check only the module. The helper records
the runtime/package version, CPU architecture, and support-source identity next
to the output; unchanged builds are reused, and rerunning Make after an NGINX
upgrade rebuilds a compatible module. Only module sources are distributed.
Students use them without editing their C source.

The helper also accepts an explicit output path:

```sh
bash foggy-http/support/nginx/install.sh "$PWD/build/ngx_stream_foggy_http_module.so"
```

With no output argument it retains the system-install behavior used by the
instructor's grader setup, writing
`/usr/lib/nginx/modules/ngx_stream_foggy_http_module.so`; that operation requires
permission to write the system module directory.
