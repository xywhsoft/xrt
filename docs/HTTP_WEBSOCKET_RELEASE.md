# HTTP / WebSocket 核心发布门禁

本门禁只冻结 XRT 内保留的 HTTP/1.1 与 WebSocket 协议底座。HTTP 客户端、服务器、路由、
拥有型报文，以及 WebSocket 连接对象、Writer、Future/TLS 适配和连接组均不属于 XRT 核心；
历史资产分别保存在 `extlibs/xhttp/archive` 与 `extlibs/xws/archive`。

HTTP/2、HTTP/3 和 QUIC 不在本阶段范围内。所有网络回归只能访问本机测试进程和回环地址。

## 生成与边界

从仓库根目录执行：

```text
python tools/generate_features.py --check
python tools/generate_api_reference.py --family http --check
python tools/generate_api_reference.py --family websocket --check
python tools/generate_example_index.py --check
python tools/amalgamate.py --check
python tools/test_scope.py
python tools/refactor_audit.py
```

`config/modules.json` 是裁剪依赖的唯一事实来源。WebSocket 核心闭包不得包含 TCP、TLS、Future、
协程、Task、Channel、连接组或任何已归档模块；HTTP 核心也不得重新引入客户端、服务器、路由、
Cookie、认证、MIME、Multipart、SSE、缓存或拥有型报文对象。

## HTTP 契约

HTTP 回归至少覆盖：

- 请求行、状态行、字段、报文长度和连接语义的严格增量解析；
- `Content-Length`、chunked、close-delimited、无正文和 tunnel 五类正文计划；
- 任意 TCP 分块、chunk extension、Trailer、Upgrade 和握手后余量；
- 调用方缓冲、字段向量和 TCP/TLS 直接提交的无强制对象写出；
- gzip、deflate、嵌套编码、输出上限、未知编码策略、回调失败和 reset；
- 畸形输入、冲突长度、整数溢出、OOM 和失败后的稳定终态。

## WebSocket 契约

WebSocket 回归至少覆盖：

- Key、Accept、版本、Upgrade、Connection、子协议和扩展字段；
- 7/16/64 位帧长、最短编码、角色 mask、原地与分片 mask；
- Text/Binary、continuation、控制帧穿插、Ping/Pong、Close 和 UTF-8；
- 消息上限、保留 opcode/RSV、畸形 Close、任意网络分块和输入余量；
- permessage-deflate offer/response、window bits、context takeover 和尾部处理；
- 流式压缩/解压、输出上限、回调失败、OOM、reset、abort 和资源归零。

核心发送路径必须能够用栈上帧头加借用 payload 直接调用网络向量发送；接收路径必须能借用网络缓冲
逐片验证和消费正文。测试不得依赖已归档连接对象来证明协议原语正确。

## 协议回归

Windows GCC x64：

```text
python tools/build.py --compiler gcc --arch x64 --suite http1_protocol_fuzz_tests,websocket_protocol_fuzz_tests,websocket_keygen --jobs 4
```

TinyCC 必须独立验证同一闭包：

```text
python tools/build.py --compiler tcc --arch x64 --suite http1_protocol_fuzz_tests,websocket_protocol_fuzz_tests,websocket_keygen --jobs 4
```

发布候选还必须执行完整模块、单头和裁剪回归：

```text
python tools/build.py --compiler gcc --arch x64 --suite all --jobs 4
python tools/build.py --compiler gcc --arch x64 --suite all --trim-only
```

Linux 和 macOS 使用 `cc` 并在真实平台运行。显式请求 io_uring 时不得静默降级到 epoll 或 select；
Windows 必须分别验证 IOCP 与 Select fallback。网络后端正确性由通用 TCP、TLS、取消、deadline、
关闭和资源归零测试承担，HTTP/WebSocket 不维护第二套后端。

## 模糊、内存与性能

普通回归始终执行固定种子和确定性分块驱动。Linux 发布环境还必须执行：

```text
python tools/test_protocol_fuzz.py --runs 100000
```

脚本必须从模块清单解析实现闭包，并使用 libFuzzer、ASan 和 UBSan。仓库内持久种子、工作语料、
崩溃工件和回流步骤见 [`fuzz/README.md`](../fuzz/README.md)。崩溃样本必须最小化并进入持久 corpus，
或转化为普通回归后才能发布。OOM 测试必须使用 `xrtMemDebugFailAfter`，并验证失败后对象、引用、
回调状态和持有内存全部归零。

体积和性能门禁：

```text
python tools/measure_size.py --profiles http_websocket --baseline dev/bench/size/SIZE_BASELINE_WINDOWS_GCC16_X64.json --check
python tools/measure_performance.py --profiles http --baseline dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
python tools/measure_performance.py --profiles websocket --baseline dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
```

WebSocket 基准只测帧解析、帧头写出和 mask 热路径；连接吞吐、TLS 和广播属于网络或 `xws` 扩展库基准。

## 发布判定

- 所有命令退出码为零，生成物与清单一致；
- 模块化、单头、裁剪和不同编译器的公开语义一致；
- ASan、UBSan、libFuzzer 和内存调试不报告越界、UAF、泄漏或未定义行为；
- 输入不足、协议错误、OOM、回调失败和超限后的状态符合文档；
- 体积与性能未超过已审核基线；
- 核心 API 能组合完整 HTTP/1.1 与 RFC 6455 流程，同时不强迫使用高级对象模型。
