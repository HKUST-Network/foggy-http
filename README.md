# ELEC 3120 Course Project: HTTP over FoggyTCP

In Checkpoint 3, you will implement HTTP over FoggyTCP and configure NGINX to
forward requests to a date service and an echo service. Use the supplied
**FoggyTCP v2** submodule rather than your CP2 implementation.

## Checkpoint 3

- HTTP/1.0 and HTTP/1.1 requests and responses.
- Persistent connections, a 3-second server idle timeout, and reconnection.
- Chunked responses with incremental delivery.
- NGINX Stream forwarding over UDP 80.

Follow the Checkpoint 3 handout for the full requirements.

## Files

The following files have been provided for you:

- `foggy-http/src/foggy_http.cc`: The HTTP library. Complete the `TODO` sections
  for message parsing, serialization, connection management, and streaming.
- `foggy-http/conf/nginx-stream.conf`: A complete NGINX configuration template.
  Fill the two `TODO` rules that map date and echo requests to their backends.
- `foggy-http/src/date-client.cc` and `date-server.cc`: The date applications.
  The server returns the current Hong Kong date and listens on UDP 8081.
- `foggy-http/src/echo-client.cc` and `echo-server.cc`: The echo applications.
  The server listens on UDP 8082; `--chunked` enables its streaming producer.
- `foggy-http/inc/foggy_http.h` and `foggy-http/src/transport.cc`: The fixed
  public API and FoggyTCP adapter. Keep these files unchanged.
- `foggy-http/support/nginx/`: The supplied C module that extracts the HTTP
  path from FoggyTCP packets for NGINX Stream routing.
- `foggy-http/utils/`: Wireshark dissectors for FoggyTCP and correctly formatted
  HTTP messages.
- `submit.py`: Packages the two editable files for Gradescope.

Modify only `foggy_http.cc` and `nginx-stream.conf`. Keep the applications,
public headers, transport, module source, and build files unchanged.
The HTTP library and applications use C11 despite their `.cc` extension.

## Build

Clone the repository and make it available in both Linux VMs:

```sh
git clone --recurse-submodules https://github.com/HKUST-Network/foggy-http.git
cd foggy-http
```

From the project root in each Debian/Ubuntu VM:

```sh
sudo bash lab/setup.sh
make -j2
```

Run setup once to install dependencies. Rebuild as an ordinary user after
editing your files. Make builds the four applications and the supplied module
at `build/ngx_stream_foggy_http_module.so`.

## Run the starter

In separate server VM terminals, from the project root:

```sh
./build/date-server
./build/echo-server
```

In the client VM:

```sh
./build/date-client 10.0.1.1 --port 8081 --no-keep-alive
./build/echo-client 10.0.1.1 hello --port 8082 --no-keep-alive
```

Replace `10.0.1.1` with your server VM's address if needed. The starter exchanges
raw data and closes after each response. It does not yet implement HTTP or
keep-alive, so use the explicit backend ports for these initial tests.

## NGINX

FoggyTCP runs over UDP, so this lab uses NGINX Stream to forward datagrams.
The supplied C module exposes the HTTP path as `$foggy_http_path`; your `map`
rules select the date backend at `127.0.0.1:8081` or echo at `127.0.0.1:8082`.

After implementing HTTP and completing the configuration, keep both servers
running and start NGINX from the project root in another server VM terminal:

```sh
sudo /usr/sbin/nginx -p "$PWD/" \
    -c "$PWD/foggy-http/conf/nginx-stream.conf" \
    -g 'pid build/nginx.pid; error_log stderr info; daemon off;'
```

Clients use UDP 80 by default:

```sh
./build/date-client 10.0.1.1
./build/echo-client 10.0.1.1 hello
```

Stop NGINX with Ctrl+C. For streaming, restart the echo server with `--chunked`.

## Submission

From the project root, rebuild and package your implementation:

```sh
make -j2
python3 submit.py
```

Submit `submit.zip` to Gradescope. It contains `foggy_http.cc`, the complete
`nginx-stream.conf`, and an automatically generated `SHA256SUMS` file at its
root. The autograder supplies the fixed code and adjusts the module paths;
do not submit compiled binaries.
