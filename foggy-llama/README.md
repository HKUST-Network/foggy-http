# CP4 · Foggy Llama

Continue from your completed CP3 project. If you received a CP4 starter ZIP,
extract it at the CP3 repository root so that `foggy-http/`, `foggy-llama/`,
and `thirdparty/` are siblings. The Fall 2026 course website provides the
CP4 handout.

## Build and try the mock service

You need a C11 and C++11 compiler, CMake 3.16 or newer, and Python 3.
On Ubuntu, install `build-essential`, `cmake`, `python3`, and `git`.
From the CP3 repository root:

```sh
git submodule update --init -- thirdparty/foggytcp
cmake -S foggy-llama -B foggy-llama/build
cmake --build foggy-llama/build -j2
```

After completing the tasks, start the service in one terminal:

```sh
./foggy-llama/build/foggy-server --mock
```

In another terminal:

```sh
./foggy-llama/build/foggy-client health
./foggy-llama/build/foggy-client models
./foggy-llama/build/foggy-client chat --prompt 'Say hello.'
```

The default is UDP port 8083 on localhost. Use `--host` and `--port` to
connect to another server. Try interactive `chat` without `--prompt` to
exercise connection reuse and the shared rate limit.

## Optional real model

The real engine needs a separate checkout of llama.cpp at the revision in
`vendor/llama-revision.txt`. It builds with the upstream compiler requirements,
including C++17. With the checkout available, run from `foggy-llama/`:

```sh
python3 vendor/fetch_model.py
cmake -S . -B build/real -DFOGGY_WITH_LLAMA=ON -DLLAMA_SOURCE_DIR=/path/to/llama.cpp
cmake --build build/real -j2
./build/real/foggy-server --model models/SmolLM2-135M-Instruct-Q4_K_M.gguf --port 8084
```

Connect with `./build/real/foggy-client --port 8084 chat --prompt 'Say hello.'`.
If model output takes longer than the CP3 read deadline, your HTTP source
can use the CP4-only `FOGGY_HTTP_READ_MS` definition for response reads.
Keep the three-second server idle timeout separate.

## Submit

From the CP3 project root, run `python3 foggy-llama/submit.py`.
Upload the resulting `foggy-llama/submit.zip`.
