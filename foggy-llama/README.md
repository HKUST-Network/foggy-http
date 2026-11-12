# CP4 · Foggy Llama

Continue from your completed CP3 project. Save your CP3 work before pulling
the CP4 update. See the [CP4 handout](https://foggynetwork.com/checkpoints/cp4)
for the assignment requirements.

Complete route registration in `foggy-llama/src/llama_routes.cc` and extend
your CP3 dispatch in `foggy-http/src/foggy_http.cc` for JSON routing errors and
`OPTIONS` queries. The model handlers, inference engines, and streaming
helpers are supplied. Both files you edit compile as C11 with the `.cc`
extension. The route registration uses the C interface in `inc/foggy_llama.h`.

## Build and try the mock service

You need GNU Make, C and C++ compilers, and Python 3.
On Ubuntu, install `build-essential`, `python3`, and `git`.
From the CP3 repository root:

```sh
git submodule update --init -- thirdparty/foggytcp thirdparty/llama.cpp
make cp4
```

After completing the tasks, start the service in one terminal:

```sh
./build/foggy-server --mock
```

In another terminal:

```sh
./build/foggy-client health
./build/foggy-client models
./build/foggy-client chat --prompt 'Say hello.'
```

The default is UDP port 8083 on localhost. Use `--host` and `--port` to
connect to another server. Try interactive `chat` without `--prompt` to
send several prompts on the same connection.

## Run the local checks

After implementing the tasks, run `make foggy-llama-check` from the repository
root. It starts a mock server on a local UDP port and checks the three model
routes, JSON 404 and 405 responses, and `OPTIONS` with `Allow`.

## Optional real model

On Ubuntu, install `cmake`, `pkg-config`, and `curl`. Build the llama.cpp
submodule and make its libraries available to the CP4 build and server. Run
from the repository root:

```sh
cmake -S thirdparty/llama.cpp -B build/llama-cmake \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/build/llama-install" \
    -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=ON \
    -DLLAMA_BUILD_COMMON=OFF -DLLAMA_BUILD_APP=OFF
cmake --build build/llama-cmake -j2
cmake --install build/llama-cmake
PKG_CONFIG_PATH="$PWD/build/llama-install/lib/pkgconfig" make cp4 FOGGY_WITH_LLAMA=1
model=$(python3 foggy-llama/vendor/fetch_model.py)
LD_LIBRARY_PATH="$PWD/build/llama-install/lib" \
    ./build/llama/foggy-server --model "$model" --port 8084
```

Connect with `./build/foggy-client --port 8084 chat --prompt 'Say hello.'`.
If model output takes longer than the CP3 read deadline, your HTTP source
can use the CP4-only `FOGGY_HTTP_READ_MS` definition for response reads.
Keep the three-second server idle timeout separate.

## Submit

From the CP3 project root, run `python3 foggy-llama/submit.py`.
Upload the resulting `foggy-llama/submit.zip` with `foggy_http.cc`,
`llama_routes.cc`, and the generated `SHA256SUMS`.
