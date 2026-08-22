# xws

`xws` 是建立在 XRT WebSocket 协议原语、网络、TLS 与 `xhttp` Upgrade
传输之上的 WebSocket 运行时扩展库。它提供已建立连接管理、客户端和服务器握手、
Copy/Ref/Take 发送、流式 Writer、Future、TLS、permessage-deflate、路由和连接组广播。

XRT 核心只保留可直接组合 TCP/TLS 的协议底座：帧解析与写出、mask、消息状态机、
Close、握手字段、子协议与扩展协商，以及流式压缩和解压。`xhttp` 提供结构化 HTTP
客户端、服务器和 Upgrade 传输。`xws` 不复制这两层能力，只负责连接建立后的完整
生命周期和经典客户端、服务器使用路径。

## 能力层次

- `websocket_connection`：直接接管 `xnettcp`，处理消息、控制帧、背压和关闭状态机。
- `websocket_writer`、`websocket_connection_ref`：流式写出与 Copy/Ref/Take 所有权路径。
- `websocket_connection_future`：可等待的发送、drain、关闭和取消。
- `websocket_client`、`websocket_server`：HTTP Upgrade 客户端与服务器接管。
- `websocket_connection_tls`：直接接管 TLS 流；`websocket_client_https` 与
  `websocket_server_tls` 提供 `wss` 经典路径。
- `websocket_connection_deflate`：连接级 permessage-deflate；客户端和服务器可独立选择。
- `websocket_group`：连接集合、快照和广播；Future 模块提供每连接结果与取消。
- `websocket_server_router`：与 `xhttp` 路由器组合，不进入基础连接闭包。

底层用户可以从 XRT 帧和消息 API 自行组合协议；需要连接状态机但不需要 HTTP 时只选择
`websocket_connection`；需要经典 `ws`/`wss` 客户端或服务器时再增加对应模块。高级层
不会阻断 `xrtWsConnTcp`、`xrtWsConnTls` 等底层访问路径。

## 使用

选择完整扩展：

```c
#define XWS_MODULE_ALL
#include <xws.h>
```

按需选择会自动展开 XRT 与 `xhttp` 依赖。例如只使用 WebSocket 客户端 Future：

```c
#define XWS_MODULE_WEBSOCKET_CLIENT_FUTURE
#include <xws.h>
```

单头模式在一个翻译单元中额外定义实现宏：

```c
#define XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_IMPLEMENTATION
#include "xws.h"
```

`single/xws.h` 包含所需 XRT 与 `xhttp` 实现，功能仍按模块宏裁剪；
`single/xws_decl.h` 只提供声明。`XWS_IMPLEMENTATION` 只能定义一次。

## 预编译 ABI

部分公开配置和统计结构会随 feature 裁剪字段。使用静态库或动态库时，库和消费者必须
采用完全相同的模块选择，否则结构布局不一致。官方完整包按 `XWS_MODULE_ALL` 构建，
消费者也必须在包含 `xws.h` 前定义 `XWS_MODULE_ALL`。自定义裁剪包应把同一组模块宏
作为构建配置的一部分同时提供给库和全部消费者。

直接编译源码或使用单头文件时，声明与实现位于同一模块配置中，不存在这项跨编译单元
配置差异。

## 发布门禁

产品闭包和示例使用 `xws`，OOM、协程、代理、平台后端与组合回归使用独立的
`xws_tests`。测试专用依赖不会进入发布库：

```text
python tools/build.py --manifest extlibs/xws/config/modules.json --suite xws --no-single
python tools/build.py --manifest extlibs/xws/config/modules.json --suite xws_tests --no-single
python tools/build.py --manifest extlibs/xws/config/modules.json --suite xws_tests --jobs 8
python tools/build.py --manifest extlibs/xws/config/modules.json --suite xws --trim-only
python tools/amalgamate.py --manifest extlibs/xws/config/modules.json --check
python tools/generate_api_reference.py --manifest extlibs/xws/config/modules.json --check
python tools/check_api_docs.py --manifest extlibs/xws/config/modules.json
python tools/measure_performance.py --config extlibs/xws/config/performance_profiles.json --manifest extlibs/xws/config/modules.json --profiles websocket_loopback --smoke
python tools/measure_size.py --config extlibs/xws/config/size_profiles.json --manifest extlibs/xws/config/modules.json --profiles * --kind single
```

公开声明位于 `include/`，实现位于 `src/`，测试和示例分别位于 `tests/` 与
`examples/`。完整符号索引见 [公共符号参考](docs/api/reference.md)。

`archive/` 不参与生成、构建、测试或发布，只保留边界重构前的历史实现和清单快照。
