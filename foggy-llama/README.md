# CP4 · Foggy Llama

Continue from your completed CP3 project. Save your CP3 work before pulling
the CP4 update. See the [CP4 handout](https://foggynetwork.com/checkpoints/cp4)
for the assignment requirements.

Complete route registration in `foggy-llama/src/llama_routes.cc` and extend
your CP3 dispatch in `foggy-http/src/foggy_http.cc` for JSON routing errors and
`OPTIONS` queries. The model handlers, inference engines, and streaming
helpers are supplied. Both files you edit compile as C11 with the `.cc`
extension. The route registration uses the C interface in `inc/foggy_llama.h`.

## Build and run

You need GNU Make, C and C++ compilers, Python 3, and Git.
On Ubuntu, install `build-essential`, `python3`, and `git`.
Run this command from the CP3 repository root.

```sh
make cp4
```

The first build initializes the submodules and downloads the course GGUF model.
On Ubuntu x64 and macOS it also downloads the pinned llama.cpp libraries. On
Ubuntu arm64 it compiles the CPU backend from the pinned source to support the
course Ubuntu 22.04 VMs.

After completing the tasks, start the service in one terminal with the mock
backend.

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

After building, run `python3 foggy-llama/tests/smoke.py` from the repository
root. It starts a mock server on a local UDP port and checks the three model
routes, JSON 404 and 405 responses, and `OPTIONS` with `Allow`.

## Run with the course model

Start the same server with the GGUF model prepared by `make cp4`.

```sh
./build/foggy-server --model foggy-llama/models/SmolLM2-135M-Instruct-Q4_K_M.gguf --port 8084
```

Connect with `./build/foggy-client --port 8084 chat --prompt 'Say hello.'`.
If model output takes longer than the CP3 read deadline, your HTTP source
can use the CP4-only `FOGGY_HTTP_READ_MS` definition for response reads.
Keep the three-second server idle timeout separate.

## Submit

From the CP3 project root, run `python3 foggy-llama/submit.py`.
Upload the resulting `foggy-llama/submit.zip` with `foggy_http.cc`,
`llama_routes.cc`, and the generated `SHA256SUMS`.
