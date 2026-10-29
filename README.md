# Foggy HTTP — ELEC 3120 CP3

在 FoggyTCP 字节流上完成 HTTP/1.x 的消息格式、连接管理和流式响应。
本仓库提供可运行的 **raw 起始版**：小文本 date/echo 可以收发，但尚未生成合法 HTTP。

## 目录

```text
foggy-http/                  # 仓库根目录
├── README.md
├── LICENSE
├── Makefile
├── submit.py
├── thirdparty/
│   └── foggytcp/             # Git submodule，跟踪 foggy-tcp-v2，提交固定
└── foggy-http/
    ├── Makefile
    ├── inc/
    │   ├── foggy_http.h      # 固定的 HTTP API
    │   ├── transport.h      # FoggyTCP 适配接口
    │   └── transport_log.h
    ├── src/
    │   ├── foggy_http.cpp    # 学生修改：HTTP 实现
    │   ├── transport.cpp    # 固定的限时读取与 EOF 适配
    │   ├── client.cpp
    │   └── server.cpp
    ├── conf/
    │   └── nginx-stream.conf # 学生修改：Nginx UDP 转发
    └── utils/
        ├── foggy-tcp.lua
        └── foggy-http.lua
```

## 获取与编译

依赖：C++17 编译器、Make、Python 3、Git。Linux 和 macOS 均可编译基础程序。

```sh
git clone --recurse-submodules https://github.com/HKUST-Network/foggy-http.git
cd foggy-http
# 如果克隆时没有初始化依赖：
git submodule update --init --recursive
make -j2
```

依赖使用 FoggyTCP 的 `foggy-tcp-v2` 分支，当前固定在
`dd30b52d8933b8c64b84991fa7c4755c9371fe53`。普通 submodule update 使用记录的提交，
不会自动切换到其他版本。不要修改 thirdparty 中的依赖代码。

## 运行起始版

在两个终端分别启动 server 和 client。端口都是 **UDP**：

```sh
# 终端 1
TZ=Asia/Hong_Kong ./build/foggy-http-server date 19081
# 终端 2
TZ=Asia/Hong_Kong ./build/foggy-http-client 127.0.0.1 19081 date --no-keep-alive
```

Echo 示例：

```sh
# 终端 1
./build/foggy-http-server echo 19082
# 终端 2
./build/foggy-http-client 127.0.0.1 19082 echo 'hello' --no-keep-alive
```

起始版每次建立新连接，GET 只发送路径，POST 只发送原始 body；服务端只读一次，
返回 raw body 并关闭。它仅支持小消息，不支持合法 HTTP、完整跨包读取或连接复用。
起始版的 chunked 选项也会先收齐数据，尚不是真正的流式输出。

## 学生实现范围

只修改 `foggy-http/src/foggy_http.cpp` 和 `foggy-http/conf/nginx-stream.conf`。
保持 inc 中的公开接口和固定 client/server、传输适配代码不变。

- 实现 GET/POST、HTTP/1.0 和 HTTP/1.1 请求/响应及方法、路径路由。
- 使用 CRLF、Host、Connection、Content-Length；正确处理分段输入与二进制 body。
- HTTP/1.1 复用连接；支持 Connection: close、3 秒空闲关闭和下一次请求重连。
- 实现 chunked 编码、增量解码和即时回调；server 的流式示例每 100 ms 生成一个 ASCII 字符。
- 完成 Nginx Stream UDP 转发：18081 → 127.0.0.1:19081，18082 → 127.0.0.1:19082。

Nginx 模板作为片段包含在 nginx.conf 的 `stream {}` 中，需要安装并启用 Stream 模块。
HTTP 客户端程序通过 FoggyTCP 访问 UDP 入口；本仓库只提供 client/server，不提供浏览器字节桥。

FoggyTCP v2 的正数读取结果表示数据，缓冲读完后的 0 表示对端 EOF；HTTP 适配层的 -2
表示读取超时。EOF 与超时不同，是否关闭本端句柄由应用决定。`foggy_close` 释放整个本地
句柄；依赖不提供 shutdown-write 半关闭接口。每个服务端实例一次处理一条会话。

## 打包与 SHA-256

在仓库根目录执行：

```sh
make -j2
python3 submit.py
python3 submit.py --verify submit.zip
unzip -l submit.zip
```

生成的扁平 ZIP 恰好有三个文件：

```text
foggy_http.cpp
nginx-stream.conf
SHA256SUMS
```

构建会编译源码快照，并把源码及 Nginx 配置的 SHA-256 记录到 `build/SHA256SUMS`。
submit.py 比较当前文件、构建快照和该记录；不一致时拒绝打包。修改后请重新构建并检查。
`--verify` 同时检查 ZIP 的文件清单、内容哈希及其与当前构建的一致性。
脚本还会打印整个 ZIP 的 SHA-256，便于记录上传的文件。哈希用于检查一致性，不是功能正确性证明。

## Wireshark

先加载传输插件，再加载 HTTP 插件；将路径替换为本机 Wireshark 的可执行文件：

```sh
wireshark \
  -X lua_script:foggy-http/utils/foggy-tcp.lua \
  -X lua_script:foggy-http/utils/foggy-http.lua \
  -r capture.pcap
```

过滤器：`foggytcp` 查看传输字段，`foggyhttp` 查看 HTTP 重组。
抓包应包含会话起点；插件用于短实验捕获，不处理序号回绕或同四元组复用的新会话。
EOF/FIN 属于传输关闭；HTTP 消息应由 Content-Length 或 chunked 终止块界定。
