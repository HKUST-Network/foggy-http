# Foggy HTTP — ELEC 3120 CP3

本实验在 FoggyTCP/UDP 上实现 HTTP/1.0、HTTP/1.1、持久连接和 chunked 响应。
四个应用分别构建，公共 HTTP 库和 FoggyTCP 库保持共享。`data` 应用提供日期服务。

| 程序 | 默认目标或监听端口 | 请求路径 |
| --- | --- | --- |
| `data-client` | 服务器 UDP 80 | GET `/checkpoint-3/date` |
| `data-server` | UDP 8081 | `/checkpoint-3/date` |
| `echo-client` | 服务器 UDP 80 | POST `/checkpoint-3/echo` |
| `echo-server` | UDP 8082 | `/checkpoint-3/echo` |

## 构建与直接访问

通过 Git 获取学生初始版时，需要同时初始化固定版本的 FoggyTCP 子模块：

```sh
git clone --recurse-submodules https://github.com/HKUST-Network/foggy-http.git
cd foggy-http
git submodule update --init --recursive
```

解压源码包后进入包含 Makefile、submit.py、foggy-http/ 和 thirdparty/ 的项目根目录：

```sh
make -j2
TZ=Asia/Hong_Kong ./build/data-server
```

在另一个服务端终端启动普通 echo 服务：

```sh
./build/echo-server
```

客户端直接访问后端时显式指定端口；用你自己的服务器 IP 替换示例：

```sh
./build/data-client 10.0.1.1 --port 8081 --no-keep-alive
./build/echo-client 10.0.1.1 hello --port 8082 --no-keep-alive
```

**初始版**只发送 raw 路径或 raw body，返回 raw 数据并关闭连接；上面的直接访问仅是小消息连通性检查。
它尚未实现 HTTP、完整消息读取、持久连接或 chunked。初始版不能通过按 HTTP 请求行路由的 NGINX。
**完成版**已经实现这些协议功能及 NGINX 路由，可用作教师测试提交。

## 学生实现范围

只修改两个文件：

- `foggy-http/src/foggy_http.cc`：HTTP 报文序列化、解析、连接管理、增量 chunked 解码和输出。
- `foggy-http/conf/nginx-stream.conf`：补全 Lua 中两个路径到后端地址的映射。

公开 C API、四个应用、FoggyTCP 及 NGINX 支持代码固定。
源码使用 `.cc` 扩展名，但 HTTP 库和应用由 `-std=c11 -x c` 编译；FoggyTCP 内部仍使用 C++。

HTTP 实现需满足：

1. GET 日期、POST echo；请求包含 Host 和 Connection，POST 包含 Content-Length（空 body 也包含）。
2. 按 method/path 路由；普通响应包含状态行和 Content-Length；二进制 body 按字节计数。
3. 缓冲跨读取的请求/响应，保留多读的字节。每行至多 8 KiB，完整 header 至多 16 KiB，body 至多 1 MiB。
4. HTTP/1.1 默认保持连接，遵守 Connection: close；HTTP/1.0 默认关闭。
5. 同一个 Client 连续请求复用连接；服务端响应完成后空闲 3 秒关闭。客户端间隔 10 秒再次 GET 时重建连接。
6. 未完成消息的读取期限为 5 秒；chunked 客户端可为每个 chunk 重新计时。
7. 用 Content-Length 或 chunked 终止块判断消息完整，持久连接不能依靠 EOF 判断每次响应结束。

## echo-server 的 chunked 选项

停止普通 echo-server，再启动：

```sh
./build/echo-server --chunked
```

服务端每 100 ms 生成一个字节，及时按 `<hex-size>\r\n<data>\r\n` 发送，最终发送 `0\r\n\r\n`。
响应使用 `Transfer-Encoding: chunked`，不带 Content-Length。没有 `--chunked` 时使用普通 Content-Length 响应。
HTTP/1.0 的流式响应可通过关闭连接结束，不发送 HTTP/1.1 chunked 编码。

echo-client 根据响应头自动解码并立即输出，**没有客户端 `--chunked` 选项，也不发送 TE: chunked**：

```sh
./build/echo-client 10.0.1.1 0123456789abcdefghij --port 8082
```

stdout 只有解码后的 body；stderr 的 `CHUNK <毫秒> <字节数>` 用于分析增量到达。
`RESPONSE <状态码> local_port=<端口>` 用于观察连接复用和重连。

## NGINX Stream + Lua

路径是 `/checkpoint-3/date`、`/checkpoint-3/echo`；`checkpoint-3` 是路径前缀。
入口是 **UDP 80**。本实验不使用普通 TCP HTTP 入口，不需要 curl 或 TCP/FoggyTCP bridge。

```text
data-client / echo-client
    -> FoggyTCP / UDP :80
    -> NGINX Stream + Lua 按 HTTP 请求路径选择后端
    -> data-server UDP :8081 / echo-server UDP :8082
```

在 Linux 服务端安装课程固定版 NGINX/OpenResty（首次编译需几分钟）：

```sh
sudo bash lab/setup.sh
```

源码包包含 OpenResty 1.27.1.2 源码及 SHA-256 校验；安装到 `/opt/foggy-nginx`，不替换系统 NGINX。
内部 C 模块只把首个数据报暴露为只读 `$foggy_packet`，不消费或修改数据；Lua 校验 FoggyTCP 头部并读取 HTTP 请求行，
将路径映射结果写入 `$foggy_backend`，再由 NGINX proxy_pass 双向转发。Lua 代码位于
`foggy-http/support/nginx/lua/foggy_router.lua`。这与 utils/ 中的 Wireshark Lua 插件运行环境不同。
安装脚本为课程 NGINX 配置绑定低端口的能力；应用程序无需 root。

实验明确采用以下范围：

- 首个 FoggyTCP 数据报包含完整 HTTP 请求行（包括 CRLF）；将完整请求行/headers 一次写入传输层即可满足两个固定路径。
- headers 剩余部分及 body 仍可能跨报文、跨读取。
- 每条 FoggyTCP/NGINX UDP 会话固定一个后端；不在同一会话中切换 date/echo。
- UDP 代理超时设为 30 秒，高于 HTTP 的 3 秒空闲超时；保留默认无限制响应数据报数量，确保 ACK、FIN 和多块数据能通过。

保持两个后端运行，在服务端另开终端：

```sh
python3 lab/nginx.py --root "$PWD/runtime/nginx" --student-config "$PWD/foggy-http/conf/nginx-stream.conf"
/opt/foggy-nginx/nginx/sbin/nginx -e stderr -t -p "$PWD/runtime/nginx/" -c "$PWD/runtime/nginx/nginx.conf"
/opt/foggy-nginx/nginx/sbin/nginx -e stderr -p "$PWD/runtime/nginx/" -c "$PWD/runtime/nginx/nginx.conf" -g 'daemon off;'
```

客户端通过默认 UDP 80 入口访问：

```sh
./build/data-client 10.0.1.1
./build/echo-client 10.0.1.1 0123456789abcdefghij
./build/data-client 10.0.1.1 --repeat 3
./build/data-client 10.0.1.1 --repeat 2 --interval 10
```

## 打包提交

```sh
make -j2
python3 submit.py
python3 submit.py --verify submit.zip
```

扁平 ZIP 恰好包含 `foggy_http.cc`、`nginx-stream.conf`、`SHA256SUMS`。
修改源码或配置后必须重新构建；打包程序拒绝与构建记录不一致的文件。
不要提交四个固定应用、NGINX 支持代码或第三方库。

## Wireshark

先加载 `foggy-http/utils/foggy-tcp.lua`，再加载 `foggy-http/utils/foggy-http.lua`。
抓包过滤器使用 `udp port 80 or udp port 8081 or udp port 8082`，服务端本机转发流量在 `lo` 或 `any` 上抓取。
这两个 Lua 是被动的 Wireshark 解码插件，不参与 NGINX 转发。
