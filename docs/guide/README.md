# XRT 使用指南

使用指南回答“选择哪一层、为什么这样组合”；精确函数签名、错误、线程和所有权契约以 [API 文档](../README.md) 为准。全部登记示例都可以从 [可运行示例索引](../EXAMPLES.md) 查找。

## 建议路线

1. 先阅读 [构建与发布](../BUILD.md)、[Core](../api/core.md)、[错误](../api/error.md) 和 [内存](../api/memory.md)，明确初始化、分配和失败模型。
2. 通过 [时间、路径与文件](time-path-file.md) 建立系统边界，再进入字符串、字符集和数值处理。
3. 按数据形态选择 Buffer、Array/Map/Set、Value、JSON 或 XSON，不让业务层反复实现容器和解析器。
4. 按[并发、协程与任务选择指南](concurrency.md)理解 Thread、Channel、Future、
   Coroutine 与 Task 的职责，再进入单个模块契约。
5. 网络应用先从 Engine、TCP/UDP、TLS 的等待、取消、背压和所有权开始，再进入 HTTP 与 WebSocket。
6. XRT 协议核心优先使用视图和调用方写出 API；拥有型对象和应用级 helper 由扩展库提供。

## 组合指南

- [时间、路径与文件](time-path-file.md)：单调时钟、路径根、编码、限长读取和原子写入。
- [XID](xid.md)：无中心标识、值类型、文本排序和严格解析。
- [Crypto](crypto.md)：Hash、KDF、AEAD、密钥交换和签名的职责边界。
- [并发、协程与任务](concurrency.md)：执行位置、消息、结果、取消、背压和结构化作用域。
- [TaskGroup 与结构化作用域](task-group.md)：动态作用域、原子启动、父子传播和收口。

组合指南只保留跨模块决策。单个模块的完整功能、常量、结构和示例不会在指南中复制，以免公共契约出现两个版本。

## 按体系阅读

| 体系 | API 起点 | 典型示例 |
| --- | --- | --- |
| 基础与文本 | [String](../api/string.md)、[Regex](../api/regex.md)、[Charset](../api/charset.md)、[Number](../api/number.md)、[Time](../api/time.md) | `examples/string`、`examples/text/regex`、`examples/number`、`examples/time` |
| 文件与进程 | [Path](../api/path.md)、[File](../api/file.md)、[异步文件](../api/file_async.md)、[Process](../api/process.md) | `examples/path`、`examples/file`、`examples/process` |
| 容器与数据 | [Buffer](../api/buffer.md)、[Typed Containers](../../extlibs/xruntime/docs/api/typed_containers.md)、[Value](../api/value.md)、[JSON](../api/json.md) | `examples/containers`、`examples/value`、`examples/data` |
| 并发与任务 | [Thread](../api/thread.md)、[Sync](../api/sync.md)、[Future](../api/future.md)、[Coroutine](../api/coroutine.md)、[Task](../api/task.md) | `examples/concurrency`、`examples/runtime` |
| 网络与 TLS | [Net](../api/net.md)、[TCP](../api/tcp.md)、[UDP](../api/udp.md)、[TLS](../api/tls.md) | `examples/network`、`examples/tls` |
| HTTP 与 WebSocket | [HTTP](../api/http.md)、[WebSocket](../api/websocket.md) | `examples/http`、`examples/websocket` |

## 查阅规则

- 需要精确返回值、限制、错误和所有权时查 API，不从示例反推契约。
- 需要最短常见写法时查示例，不在业务代码复制测试内部 helper。
- 需要实现原理、平台差异和取舍时查 `docs/design`。
- 需要确认裁剪、单头和平台状态时查 [功能选择](../FEATURE_SELECTION.md) 与 [发布状态](../RELEASE_STATUS.md)。

当前使用指南以中文为权威来源。函数签名、编译条件和精确错误语义始终以公开头文件与 API 参考为准。
