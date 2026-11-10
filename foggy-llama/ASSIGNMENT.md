# CP4 · Foggy Llama 任务说明

使用 Foggy HTTP 和 FoggyTCP v2，为单个本地文本模型实现请求分发、业务路由和限流。课程提供路由表和注册函数、HTTP framing、SSE 编解码、推理适配器、客户端模型选择和多轮历史。

## 一 · 分发 HTTP 请求

在 `src/router.cpp` 中实现 `dispatch_request()`。框架提供已解析的请求和已初始化的响应。

- 遍历 `server->routes`，按 HTTP 方法和路径精确匹配，区分大小写。匹配时取 `request->target` 中第一个 `?` 前的路径。例如 `GET /health?probe=1` 匹配 `GET /health`。
- 匹配成功时，把原始请求、响应和路由的 `context` 传给 handler，并返回它的结果。同一路径可以分别注册 GET 和 POST。
- 路径不存在时返回 JSON 404。路径存在但方法不匹配时返回 JSON 405，并设置该路径的 `Allow` 响应头。取值为 `GET`、`POST` 或 `GET, POST`。使用 `error_response()` 和 `foggy_http_header_set()`，成功返回 `0`，helper 失败时返回 `-1`。

测试会注册自定义路径来验证通用分发。

## 二 · 实现业务路由

在 `src/llama_routes.cpp` 中完成以下函数。

1. **健康检查**。在 `health()` 里从 `context` 取得 `ServiceContext`，从 `service.engine.model_info()` 读取当前模型的 `id`，返回 HTTP 200 和 `{"status":"ok","model":"<alias>"}`。查询抛出异常时返回 JSON 503。检查只读取元数据。
2. **模型列表**。在 `models()` 里读取 `engine.model_info()`，将完整模型对象放进 `{"object":"list","data":[模型对象]}`，通过 `json_response()` 返回 HTTP 200。读取模型信息失败时返回 500。
3. **流式聊天**。在 `chat()` 里用 `parse_chat(*request, engine.model_info())` 校验并取得 `Chat`，通过限流检查后调用 `engine.start_chat()` 和 `chat_response()`。校验失败时停止处理。用给出的 `failure()` 把 `std::invalid_argument` 转为 HTTP 400，其他异常转为 HTTP 500。
4. **绑定路由**。在 `register_llama_routes()` 里注册 `GET /health`、`GET /v1/models` 和 `POST /v1/chat/completions`。给三个回调都传入 `&service`。注册失败返回 `-1`，成功返回 `0`。

HTTP C 回调中的异常应在回调内捕获。引擎的生命周期覆盖服务端。请求缓冲区仅在回调期间有效，`parse_chat()` 会复制所需字段，`chat_response()` 会保留流式生成对象。

示例聊天请求如下。校验器接受文本 `system`、`user`、`assistant` 消息，最后一条须为 `user`。模型 ID 必须与服务端 alias 一致，`max_tokens` 支持 1–128，默认 64，`stream` 默认为 `true`，也可显式传入 `true`。

```json
{
  "model": "course-model",
  "messages": [{"role": "user", "content": "解释 keep-alive"}],
  "max_tokens": 64,
  "stream": true
}
```

`chat_response()` 生成 SSE 文本增量、`finish_reason` 和 `[DONE]`，HTTP 框架负责 chunked 传输。客户端收到完整成功的流以后保存这一轮的历史。

## 三 · 聊天请求限流

在 `src/rate_limiter.cpp` 中完成 `ChatRateLimiter::admit(now)`，使用传入的单调时钟时间点。

- 第一次放行开启 10 秒窗口，每个窗口最多放行 3 次。放行时计数并返回 `0`。
- 配额用完时，返回距离窗口结束的秒数，向上取整。拒绝请求时保留原计数和窗口。到达或超过窗口截止时刻的请求开启新窗口。
- 在聊天 handler 中先调用 `parse_chat()`，再调用 `service.limiter.admit(service.now())`。返回正数时生成 JSON 429，并将该整数填入 `Retry-After`。放行后才调用引擎。
- 同一服务端的连接共享配额。健康检查、模型列表和校验失败的请求不占用配额。已放行的请求在引擎报错时仍占用配额。

例如，前三条有效请求同时到达，1.5 秒后的第四条请求收到 `429` 和 `Retry-After: 9`。第 10 秒到达的请求开启新窗口。

## 自动验收

| 检查 | 分数 |
| --- | ---: |
| 路由匹配、404、405、`Allow` 和错误后的连接复用 | 30 |
| 健康检查、模型列表、聊天校验与 SSE，以及 FoggyTCP 集成 | 40 |
| 固定窗口、聊天限流、HTTP 429 和错误后的连接复用 | 30 |

Gradescope 将 ZIP 内的三个源文件放入课程固定框架中编译。限流边界由可控时钟验收，FoggyTCP 环回测试会检查 `405` 和 `429` 之后能否在原连接上继续访问。真实模型运行见 README。

构建和运行命令见 [README](README.md)。完成后运行 `python3 submit.py`，上传 `submit.zip`。无需提交报告。
