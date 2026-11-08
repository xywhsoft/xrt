# xws

已建立连接的收发运行时。接管已经开放的 TCP/TLS Stream，处理消息、控制帧、关闭、发送预算与异步操作。Writer、引用发送、Future、TLS 和压缩仍为可选模块，共享连接生命周期。

## 边界与依赖

XRT。每个扩展的单头只包含自身实现；单头依赖由宿主按顺序提供，整个程序使用同一个 XRT。

## 模块

| 模块 | 选择宏 |
| --- | --- |
| `websocket_connection` | `XWS_MODULE_WEBSOCKET_CONNECTION` |
| `websocket_connection_ref` | `XWS_MODULE_WEBSOCKET_CONNECTION_REF` |
| `websocket_writer` | `XWS_MODULE_WEBSOCKET_WRITER` |
| `websocket_writer_ref` | `XWS_MODULE_WEBSOCKET_WRITER_REF` |
| `websocket_connection_future` | `XWS_MODULE_WEBSOCKET_CONNECTION_FUTURE` |
| `websocket_connection_ref_future` | `XWS_MODULE_WEBSOCKET_CONNECTION_REF_FUTURE` |
| `websocket_connection_tls` | `XWS_MODULE_WEBSOCKET_CONNECTION_TLS` |
| `websocket_connection_deflate` | `XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE` |
| `websocket_writer_deflate` | `XWS_MODULE_WEBSOCKET_WRITER_DEFLATE` |

`XWS_MODULE_ALL` 选择本库全部现有能力。普通使用从 `XWS_MODULE_WEBSOCKET_CONNECTION` 开始，只按需开启可选模块。

## 构建与验证

```powershell
python tools/build.py --manifest extlibs/xws/config/modules.json --suite websocket_connection --no-single --no-examples
python tools/build.py --manifest extlibs/xws/config/modules.json --suite xws_tests --no-examples
python tools/amalgamate.py --manifest extlibs/xws/config/modules.json
python tools/package.py --manifest extlibs/xws/config/modules.json --suite xws --kind static --verify
```

源码入口为 `include/xws.h`；单头为 `single/extlibs/xws.h`，声明头为 `single/extlibs/xws_decl.h`。模块宏必须在首次包含依赖特性头之前设置。单头实现只在一个编译单元定义各库的 `*_IMPLEMENTATION`。

发布工具分别生成本库、所选依赖与 XRT 的库文件，并写入 `packages.json`。静态宿主只链接一份 Core；动态消费者定义 `XWS_USE_SHARED` 以及所选依赖的对应宏。构建本库动态库使用 `XWS_BUILD_SHARED`。

## 迁移与所有权

从原 xws 拆分时，连接和 Writer API 名称保持不变。客户端、服务端、分组分别包含 `<xws-client.h>`、`<xws-server.h>`、`<xws-group.h>`，使用各库选择宏。旧 `websocket_http_future` 聚合模块已取消。

客户端 Future 成功值为 `xwsclientresult`，使用 `xrtWsClientResult*`；服务端 Future 成功值为 `xwsserverresult`，使用 `xrtWsServerResult*`。只有客户端结果持有 101 Response。两端结果均支持引用、借用连接、原子取走连接；释放最后一个结果引用时中止尚未取走的连接。

公共 WebSocket 配置和握手结构的布局不随 Future、TLS 或压缩模块改变；内部可选状态保留在不透明对象中。未启用的压缩能力会明确拒绝配置。更新后应重编译库和消费者；HTTP 参数仍遵守对应 HTTP 库的构建契约。

事件在所属网络 Worker 上执行。消息视图仅在回调期间借用；Copy 复制数据，Ref 保留引用，Take 按原契约转移所有权。发送预算和异步队列有硬上限，控制帧预留、取消、失败清理与关闭顺序保持原行为。

公开结构体参数中明确允许未对齐存储的配置、事件、Ref、Deflate 和 CloseInfo 按字节复制；调用方仍须提供完整有效范围，并遵守对应接口的重叠与所有权约束。`xws_tests` 的单头回归测试复用模块化的真实回环、失败不发布和引用释放检查。

## 回环应用示例

```powershell
python tools/build.py --compiler gcc --manifest extlibs/xws/config/modules.json --suite xws_runtime_loopback_example
```

[`runtime_loopback/main.c`](examples/websocket/runtime_loopback/main.c) 在本机动态端口建立两个真实 TCP Stream，并在各自 Worker 的 Open/Accept 回调中接管连接。两端预先约定 `loopback.v1` 与 permessage-deflate 配置；HTTP Upgrade 集成由 xws-client/xws-server 提供。

业务线程依次异步提交复制文本、所有权文本、压缩文本。服务端在消息回调中使用普通 Writer、引用 Writer 和压缩 Writer 分片回送；两端核对完整文本、消息顺序和压缩标志。示例还核对 Ping/Pong、双方干净关闭，以及客户端一个 Ref 和服务端两个引用分片共三次释放。关闭后释放应用引用，等待 Worker 退休并检查 `LiveObjects == 0`，再销毁 Engine 和回调上下文。等待均有十秒上限，失败返回非零。

该命令同时运行模块化示例和复用相同业务过程的实现单头测试，无需外部服务。成功输出包含 `releases=3 live=0`。其余 `connection_ref`、`writer*`、`connection*_future` 目录展示供宿主调用的集成函数；`session_channel_coroutine` 展示 Channel 消费过程，它们的独立 main 只验证片段构建和初始化。

详细接口见 [API 参考](docs/api/reference.md)，整体边界见 [WebSocket 拆分说明](docs/design/websocket-runtime.md)。
