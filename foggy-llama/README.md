# CP4 · Foggy Llama

Continue from your completed CP3 project. If you received a CP4 starter ZIP,
extract it at the CP3 repository root so that `foggy-http/`, `foggy-llama/`,
and `thirdparty/` are siblings. The Fall 2026 course website provides the
CP4 handout.

Complete route registration in `foggy-llama/src/llama_routes.cc` and extend
your CP3 dispatch in `foggy-http/src/foggy_http.cc` for JSON routing errors and
`OPTIONS` queries. The model handlers, inference engines, and streaming
helpers are supplied.

## Build and try the mock service

You need GNU Make, C and C++ compilers, and Python 3.
On Ubuntu, install `build-essential`, `python3`, and `git`.
From the CP3 repository root:

```sh
git submodule update --init -- thirdparty/foggytcp
make -j2 foggy-llama
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

Install the llama.cpp headers and libraries built from the commit in
`vendor/llama-revision.txt`. Make reads their compiler and linker flags from
`pkg-config llama`. For a custom installation, set `PKG_CONFIG_PATH` and make
its shared libraries visible to the system loader. Run from the repository root:

```sh
python3 foggy-llama/vendor/fetch_model.py
make -j2 foggy-llama FOGGY_WITH_LLAMA=1
./build/llama/foggy-server --model foggy-llama/models/SmolLM2-135M-Instruct-Q4_K_M.gguf --port 8084
```

Connect with `./build/foggy-client --port 8084 chat --prompt 'Say hello.'`.
If model output takes longer than the CP3 read deadline, your HTTP source
can use the CP4-only `FOGGY_HTTP_READ_MS` definition for response reads.
Keep the three-second server idle timeout separate.

## Submit

From the CP3 project root, run `python3 foggy-llama/submit.py`.
Upload the resulting `foggy-llama/submit.zip` with `foggy_http.cc`,
`llama_routes.cc`, and the generated `SHA256SUMS`.
