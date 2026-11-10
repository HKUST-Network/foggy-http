# CP4 · Foggy Llama

完成 [任务说明](ASSIGNMENT.md) 中的三个文件。

| 文件 | 任务 |
| --- | --- |
| `src/router.cpp` | 分发 HTTP 请求，返回 404 或 405 |
| `src/llama_routes.cpp` | 实现健康检查、模型列表和流式聊天 |
| `src/rate_limiter.cpp` | 实现聊天限流 |

`src/` 也包含课程提供的应用入口、HTTP 分发接口和推理适配器。传输层复用仓库中的 CP3 FoggyTCP 和 transport adapter。

## 构建和运行

需要 C11 和 C++17 编译器、CMake 3.16+ 和 Python 3。Ubuntu 可用 `sudo apt install build-essential cmake python3 git curl` 安装依赖。

在原 CP3 仓库根目录执行下面的命令。

```sh
git pull --no-rebase https://github.com/HKUST-Network/foggy-http.git main
git submodule update --init -- thirdparty/foggytcp
cd foggy-llama
cmake -S . -B build
cmake --build build -j2
```

补全学生文件后，在两个终端分别启动服务端和客户端。

```sh
./build/foggy-server --mock
```

```sh
./build/foggy-client health
./build/foggy-client chat --prompt '你好'
```

客户端默认连接本机 UDP 8083。连接另一台机器时，使用 `--host` 指定服务端地址。

## 可选 · 真实模型

从 `foggy-llama/` 运行以下命令。仓库根目录的 `thirdparty/llama.cpp` 固定在 `1ab7e5ad2`，模型下载脚本校验 GGUF 的 SHA-256。

```sh
git -C .. submodule update --init --depth 1 -- thirdparty/llama.cpp
python3 vendor/fetch_model.py
cmake -S . -B build/real -DFOGGY_WITH_LLAMA=ON
cmake --build build/real -j2
./build/real/foggy-server --model models/SmolLM2-135M-Instruct-Q4_K_M.gguf --port 8084
```

用 `./build/real/foggy-client --port 8084 chat --prompt '你好'` 连接真实模型。

## 提交

在 `foggy-llama/` 运行 `python3 submit.py`，将生成的 `submit.zip` 上传至 Gradescope。只提交代码，无需报告。
