# Sources and licenses

- `json.hpp`: nlohmann/json 3.12.0, copied from the pinned course llama.cpp checkout's `vendor/nlohmann/json.hpp`. MIT license; see `json-LICENSE`.
- `llama-revision.txt` pins the llama.cpp headers and `llama-release.json` pins the matching runtime libraries and checksums. `llama-LICENSE` is the upstream MIT license. [llama.cpp b11177 release](https://github.com/ggml-org/llama.cpp/releases/tag/b11177)
- The FoggyTCP v2 transport and HTTP C API come from the current CP3 package. Their original course distribution restrictions still apply.
- CP4's model adapter and JSON/SSE helpers are based on the earlier course CP4 implementation.
