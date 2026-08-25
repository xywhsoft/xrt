# XRT 文档

- [构建、单头与库发布](BUILD.md)
- [发布状态与平台证据](RELEASE_STATUS.md)
- [六个扩展库最终发布审计](EXTENSION_RELEASE_AUDIT.md)
- [全部可运行示例](EXAMPLES.md)
- [使用指南与建议学习路线](guide/README.md)

本目录记录 XRT 2.0 的公共契约、裁剪边界、实现分层和发布门禁。公共 API 只以当前
头文件和对应 API 文档为准；`dev/ver1` 仅是只读历史资产，不代表 2.0 契约。

## 工程与发布

- [架构与目录](ARCHITECTURE.md)
- [代码风格](CODE_STYLE.md)
- [重构审计记录](REFACTOR.md)
- [旧版与新版代码及功能对应审计](REFACTOR_CORRESPONDENCE_AUDIT.md)
- [产品边界与功能准入](SCOPE.md)
- [功能选择与裁剪](FEATURE_SELECTION.md)
- [性能基准与发布规则](PERFORMANCE.md)
- [全库高性能路径评估](PERFORMANCE_AUDIT.md)
- [HTTP/1.1 与 WebSocket 发布门禁](HTTP_WEBSOCKET_RELEASE.md)
- [协议模糊测试与语料回流](../fuzz/README.md)
- [第三方实现与许可证](THIRD_PARTY.md)

## 基础与数据

- [Core](api/core.md)、[错误](api/error.md)、[内存](api/memory.md)、
  [内存调试](api/memory_debug.md)、[内存统计](api/memory_stats.md)
- [字符串](api/string.md)、[字符集](api/charset.md)、[数字](api/number.md)、
  [时间](api/time.md)、[数学](api/math.md)、[随机数](api/random.md)
- [Buffer](api/buffer.md)、[Array](api/array.md)、[Map](api/map.md)、
  [Set](api/set.md)、[Queue](api/queue.md)、[Stack](api/stack.md)、
  [AVL](api/avl.md)、[Slot Map](api/slot_map.md)
- [Value](api/value.md)、[JSON](api/json.md)、[XSON](api/xson.md)、
  [Typed Containers](../extlibs/xruntime/docs/api/typed_containers.md)
- [Hash](api/hash.md)、[Codec](api/codec.md)、[HTML 转义](api/html.md)、
  [压缩](api/compress.md)、
  [Crypto](api/crypto.md)、[ASN.1](api/asn1.md)、[PEM](api/pem.md)、
  [X.509](api/x509.md)
- [XID 使用指南](guide/xid.md)、[Crypto 组合指南](guide/crypto.md)

## 系统与并发

- [路径](api/path.md)、[文件](api/file.md)、[异步文件](api/file_async.md)、
[临时资源](api/temp.md)、[控制台](api/console.md)、[环境变量](api/environment.md)、[进程信号](api/signal.md)、[进程](api/process.md)、[日志](api/logger.md)
- [时间、路径与文件使用指南](guide/time-path-file.md)
- [原子操作](api/atomic.md)、[线程](api/thread.md)、[同步原语](api/sync.md)、
  [Once](api/once.md)、[线程局部存储](api/thread-key.md)
- [取消](api/cancel.md)、[等待](api/wait.md)、[Future](api/future.md)、
  [协程](api/coroutine.md)、[Channel](api/channel.md)、[任务](api/task.md)
- [并发、协程与任务选择指南](guide/concurrency.md)
- [TaskGroup 与结构化作用域](guide/task-group.md)

## 网络与 TLS

- [网络底座](api/net.md)、[网络公共符号参考](api/net-reference.md)、
  [网络接口](api/net-interface.md)、[DNS](api/net-dns.md)、
  [Resolver](api/net-resolver.md)、[Proxy](api/proxy.md)
- [TCP](api/tcp.md)、[UDP](api/udp.md)、[TLS](api/tls.md)、
  [TLS 公共符号参考](api/tls-reference.md)
- [TLS 会话设计](design/tls-session.md)

## URL 与 HTTP 协议

协议工具是可选的数据解析与构建层，不要求客户端或服务器先创建拥有型对象。
需要保留原始线缆形式、直接回复固定内容或自行组合数据时，可以只使用对应的低级
视图和写出 API。

- [URL](../extlibs/xhttp/docs/api/url.md)、[原始 Query](../extlibs/xhttp/docs/api/query.md)、
  [Query 参数](../extlibs/xhttp/docs/api/query_params.md)、[表单](../extlibs/xhttp/docs/api/form.md)
- [HTTP 协议底座与 HTTP/1.1](api/http.md)、
  [字段与参数](api/http_fields.md)、[内容编码](api/http_encoding.md)、
  [正文自动解码](api/http_decode.md)
- [Expect](api/http_expect.md)、[Upgrade](api/http_upgrade.md)、
  [TE](api/http_te.md)、[Connection](api/http_connection.md)、
  [Trailer](api/http_trailer.md)

HTTP 高级能力位于独立扩展 `extlibs/xhttp`，拥有自己的构建与发布门禁；其中的
`archive/` 只保存不参与产品构建的历史快照。

## WebSocket

- [WebSocket 协议底座](api/websocket.md)、[WebSocket 公共符号参考](api/websocket-reference.md)
- [HTTP 核心设计](design/http-runtime.md)、[HTTP 公共符号参考](api/http-reference.md)

WebSocket 连接对象、Writer、Future/TLS 适配、连接组、客户端、服务器与 HTTP 适配位于独立
扩展 `extlibs/xws`，不进入 XRT 核心闭包；其中的 `archive/` 只保存历史快照。

## 通用运行时模型

- [运行时类型](../extlibs/xruntime/docs/api/runtime_type.md)、[运行时值](../extlibs/xruntime/docs/api/runtime_value.md)、
  [对象](../extlibs/xruntime/docs/api/runtime_object.md)、[对象图](../extlibs/xruntime/docs/api/runtime_object_graph.md)
- [字段](../extlibs/xruntime/docs/api/runtime_field.md)、[动态字段](../extlibs/xruntime/docs/api/runtime_dynamic_field.md)、
  [调用](../extlibs/xruntime/docs/api/runtime_call.md)
- [运行时对象模型](../extlibs/xruntime/docs/design/runtime_object_model.md)

每份 API 文档都应包含裁剪宏、依赖、所有权、错误语义、线程规则和可运行范例。
范例源码按体系位于仓库的 `examples/` 目录，测试与边界证据位于 `tests/`。
