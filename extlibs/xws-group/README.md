# xws-group

连接集合和有界广播。持有成员引用、生成稳定快照、提交有界广播并报告逐连接结果。分组不管理监听、HTTP、鉴权、房间、业务消息或持久化。

## 边界与依赖

XRT → xws。每个扩展的单头只包含自身实现；单头依赖由宿主按顺序提供，整个程序使用同一个 XRT。

## 模块

| 模块 | 选择宏 |
| --- | --- |
| `websocket_group` | `XWS_GROUP_MODULE_WEBSOCKET_GROUP` |
| `websocket_group_future` | `XWS_GROUP_MODULE_WEBSOCKET_GROUP_FUTURE` |

`XWS_GROUP_MODULE_ALL` 选择本库全部现有能力。普通使用从 `XWS_GROUP_MODULE_WEBSOCKET_GROUP` 开始，只按需开启可选模块。

## 构建与验证

```powershell
python tools/build.py --manifest extlibs/xws-group/config/modules.json --suite websocket_group --no-single --no-examples
python tools/build.py --manifest extlibs/xws-group/config/modules.json --suite xws_group_tests --no-examples
python tools/amalgamate.py --manifest extlibs/xws-group/config/modules.json
python tools/package.py --manifest extlibs/xws-group/config/modules.json --suite xws_group --kind static --verify
```

源码入口为 `include/xws-group.h`；单头为 `single/extlibs/xws-group.h`，声明头为 `single/extlibs/xws-group_decl.h`。模块宏必须在首次包含依赖特性头之前设置。单头实现只在一个编译单元定义各库的 `*_IMPLEMENTATION`。

发布工具分别生成本库、所选依赖与 XRT 的库文件，并写入 `packages.json`。静态宿主只链接一份 Core；动态消费者定义 `XWS_GROUP_USE_SHARED` 以及所选依赖的对应宏。构建本库动态库使用 `XWS_GROUP_BUILD_SHARED`。

## 迁移与所有权

从原 xws 拆分时，连接和 Writer API 名称保持不变。客户端、服务端、分组分别包含 `<xws-client.h>`、`<xws-server.h>`、`<xws-group.h>`，使用各库选择宏。旧 `websocket_http_future` 聚合模块已取消。

客户端 Future 成功值为 `xwsclientresult`，使用 `xrtWsClientResult*`；服务端 Future 成功值为 `xwsserverresult`，使用 `xrtWsServerResult*`。只有客户端结果持有 101 Response。两端结果均支持引用、借用连接、原子取走连接；释放最后一个结果引用时中止尚未取走的连接。

公共 WebSocket 配置和握手结构的布局不随 Future、TLS 或压缩模块改变；内部可选状态保留在不透明对象中。未启用的压缩能力会明确拒绝配置。更新后应重编译库和消费者；HTTP 参数仍遵守对应 HTTP 库的构建契约。

事件在所属网络 Worker 上执行。消息视图仅在回调期间借用；Copy 复制数据，Ref 保留引用，Take 按原契约转移所有权。发送预算和异步队列有硬上限，控制帧预留、取消、失败清理与关闭顺序保持原行为。

详细接口见 [API 参考](docs/api/reference.md)，整体边界见 [WebSocket 拆分说明](../xws/docs/design/websocket-runtime.md)。
